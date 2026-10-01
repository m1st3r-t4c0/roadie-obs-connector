#include <browser-panel.hpp>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QDir>
#include <cstdio>
#include <memory>

class BrowserOwner : public QWidget {
public:
  QCefWidget *browser = nullptr;
  ~BrowserOwner() override { std::puts("PROBE owner destructor"); delete browser; }
};

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
  const bool preclose = std::string(argv[4]) == "preclose";
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
  app.exec();
  obs_shutdown();
  std::puts("PROBE clean shutdown");
  return 0;
}
