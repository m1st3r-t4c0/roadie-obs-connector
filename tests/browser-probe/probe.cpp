#include <browser-panel.hpp>
#include "script-gate.hpp"
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QDir>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

class BrowserOwner : public QWidget {
public:
  QCefWidget *browser = nullptr;
  ~BrowserOwner() override { std::puts("PROBE owner destructor"); delete browser; }
};

class Relay : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  std::function<void()> onUrl, onTitle;
public slots:
  void url(const QString &) { onUrl(); }
  void title(const QString &) { onTitle(); }
};

// #304: the plugin's timers call executeJavaScript every few milliseconds while the page reloads,
// changes URL, sits under a nested dialog loop, and closes. "gate" routes every call through the
// plugin's ScriptGate (src/script-gate.hpp) and counts what it sent and dropped in each phase;
// "ungated" makes the same calls straight into obs-browser, as the plugin did on Windows before.
struct GateRun {
  static constexpr const char *phases[] = {"load", "reload", "navigate", "dialog", "close"};
  int phase = 0;
  long sent[5] = {};
  long dropped[5] = {};
  int urls[5] = {};
  int titles[5] = {};
  void report() const
  {
    for (int i = 0; i < 5; ++i)
      std::printf("PROBE gate phase=%s sent=%ld dropped=%ld urls=%d titles=%d\n", phases[i], sent[i], dropped[i],
                  urls[i], titles[i]);
    std::fflush(stdout);
  }
};

static void runGate(QApplication &app, QMainWindow &main, QDockWidget *dock, BrowserOwner *owner,
                    bool gated, std::unique_ptr<QCefCookieManager> &cookies,
                    std::unique_ptr<QCef> &cef)
{
  auto *run = new GateRun;
  auto *gate = new ScriptGate;
  gate->loading(); // as SignalDock: shut until the first page reports its title
  QCefWidget *browser = owner->browser;
  // String-based connections, as the plugin makes them: obs-browser exports no metaobject symbols.
  auto *relay = new Relay(&app);
  relay->onUrl = [run, gate] {
    ++run->urls[run->phase];
    gate->loading();
  };
  relay->onTitle = [run, gate] {
    ++run->titles[run->phase];
    gate->titleSeen();
  };
  QObject::connect(browser, SIGNAL(urlChanged(QString)), relay, SLOT(url(QString)));
  QObject::connect(browser, SIGNAL(titleChanged(QString)), relay, SLOT(title(QString)));
  // The plugin's 100 ms audio timer, much faster, so the not-ready window is hit every time.
  auto *hammer = new QTimer(&app);
  hammer->setInterval(2);
  QObject::connect(hammer, &QTimer::timeout, &app, [run, gate, browser, gated] {
    if (gated && !gate->open()) {
      ++run->dropped[run->phase];
      return;
    }
    browser->executeJavaScript("void window.roadieProbe;");
    ++run->sent[run->phase];
  });
  hammer->start();
  QTimer::singleShot(3000, &app, [run, browser] {
    run->phase = 1;
    std::puts("PROBE phase reload");
    browser->executeJavaScript("location.reload();");
  });
  QTimer::singleShot(6000, &app, [run, browser] {
    run->phase = 2;
    std::puts("PROBE phase navigate");
    // Another site (localhost, not 127.0.0.1), so Chromium swaps the page into a new renderer.
    browser->executeJavaScript("location.href='http://localhost:'+location.port+'/gate-next';");
  });
  QTimer::singleShot(9000, &app, [run, &main] {
    run->phase = 3;
    std::puts("PROBE phase dialog");
    std::fflush(stdout);
    QDialog dialog(&main);
    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    QTimer::singleShot(1000, &dialog, &QDialog::accept);
    dialog.exec();
    std::puts("PROBE dialog closed");
  });
  QTimer::singleShot(12000, &app, [run, gate, browser] {
    run->phase = 4;
    std::puts("PROBE phase close");
    std::fflush(stdout);
    gate->close();
    browser->closeBrowser();
  });
  QTimer::singleShot(13500, &app, [&app, run, hammer, dock, &cookies, &cef] {
    hammer->stop();
    run->report();
    delete dock;
    std::puts("PROBE dock destroyed");
    cookies->FlushStore();
    cookies.reset();
    cef.reset();
    QTimer::singleShot(500, &app, &QApplication::quit);
  });
}

int main(int argc, char **argv)
{
  if (argc != 5) return 2;
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  std::printf("PROBE OBS %s Qt %s\n", obs_get_version_string(), qVersion());
  const QString host = QString::fromLocal8Bit(argv[1]);
  const QByteArray profile = QDir(QString::fromLocal8Bit(argv[2])).absolutePath().toUtf8();
  QDir().mkpath(QString::fromUtf8(profile));
  const std::string url = argv[3];
  const std::string mode = argv[4];
  const bool preclose = mode == "preclose";
  if (!obs_startup("en-US", profile.constData(), nullptr)) return 3;
  obs_module_t *module = nullptr;
  const QByteArray dll = (host + "/obs-plugins/64bit/obs-browser.dll").toUtf8();
  const QByteArray data = (host + "/data/obs-plugins/obs-browser").toUtf8();
  if (obs_open_module(&module, dll.constData(), data.constData()) != MODULE_SUCCESS || !obs_init_module(module)) return 4;
  std::unique_ptr<QCef> cef(obs_browser_init_panel());
  if (!cef) return 5;
  cef->init_browser();
  if (!cef->wait_for_browser_init()) return 6;
  std::unique_ptr<QCefCookieManager> cookies(cef->create_cookie_manager("roadie-signal/browser-cookies", true));
  if (!cookies) return 7;
  QMainWindow main;
  main.setAttribute(Qt::WA_DontShowOnScreen);
  auto *dock = new QDockWidget("Probe", &main);
  auto *owner = new BrowserOwner;
  auto *layout = new QVBoxLayout(owner);
  owner->browser = cef->create_widget(owner, url, cookies.get());
  layout->addWidget(owner->browser);
  dock->setWidget(owner);
  main.addDockWidget(Qt::RightDockWidgetArea, dock);
  main.resize(400, 300);
  main.show();
  if (mode == "gate" || mode == "ungated") {
    runGate(app, main, dock, owner, mode == "gate", cookies, cef);
  } else {
    QTimer::singleShot(4000, &app, [&] { std::printf("PROBE flush %d\n", cookies->FlushStore()); });
    QTimer::singleShot(5000, &app, [&] {
      std::printf("PROBE closing mode=%s\n", preclose ? "preclose" : "parent-delete");
      std::fflush(stdout);
      QTimer::singleShot(0, &main, [&main] {
        std::puts("PROBE pending main-window paint");
        std::fflush(stdout);
        main.resize(420, 320);
        main.grab();
        std::puts("PROBE paint finished");
      });
      if (preclose) owner->browser->closeBrowser();
      delete dock;
      std::puts("PROBE dock destroyed");
      cookies->FlushStore();
      cookies.reset();
      cef.reset();
      QTimer::singleShot(500, &app, &QApplication::quit);
    });
  }
  app.exec();
  obs_shutdown();
  std::puts("PROBE clean shutdown");
  return 0;
}

#include "probe.moc"
