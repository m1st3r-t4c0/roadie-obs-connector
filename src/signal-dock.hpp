#pragma once
#include "audio-tap.hpp"
#include "script-gate.hpp"
#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTimer>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <memory>

struct QCef;
class QCefWidget;
struct QCefCookieManager;

// While OBS is closing, while a modal dialog is open (including native macOS alerts), or from
// inside a nested event loop, the dock page may already be gone. Script delivery to the pages
// pauses then, on every platform (macOS since 0.3.3, Windows since #304).
void roadieSetScriptsPaused(bool paused);
// True when it is safe to call into the dock pages right now.
bool roadieScriptsSafe();

class SignalDock final : public QWidget {
  Q_OBJECT
public:
  // One page per Roadie server, all in the one Roadie Signal dock. The pages share one
  // browser cookie store (owned by the plugin), so a page that runs out of sight is
  // already signed in to Twitch and Roadie's own sign-in can finish without a click.
  SignalDock(QCef *cef, const QString &origin, QCefCookieManager *cookies, QWidget *parent = nullptr);
  ~SignalDock() override;
  bool initialized() const;
  void prepareForShutdown();
  // Re-reads the saved microphone, after the Roadie Signal dialog changed it.
  void reloadMicrophone();
  // Whether this server receives OBS data, and whether its OBS commands are followed.
  void setRoles(bool sends, bool obeys);
  // A page nobody is looking at signs itself in: on Roadie's sign-in screen it follows
  // the Twitch login link by itself, at most once a minute.
  void setBackground(bool background);
  // On the trusted /dock page (signed in or showing Roadie's own sign-in screen).
  bool onDock() const { return trustedPage_; }
  // Where the page is, for the settings window: empty while it is on /dock.
  QString elsewhere() const { return trustedPage_ ? QString() : lastHost_; }
  void publishState();
  void publishHealth();
  // 1.1.0: the layout of the scene on air, sent only when it changes (and every five seconds, so a
  // page or server that missed one catches up).
  void publishLayout();

signals:
  void pageChanged();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void navigationChanged(const QString &url);
  void titleChanged(const QString &title);

private:
  // #304: while the page is not ready, a timer's frame is dropped and an answer the page waits
  // for (a command result, a part's acknowledgement, update progress) is held.
  enum class Delivery { Drop, Hold };
  static constexpr int maxHeld = 32;
  bool scriptsSafe() const;
  // True when the script went into the page now.
  bool runScript(const QByteArray &script, Delivery delivery);
  void deliverHeld();
  void publish();
  void commandPart(const QString &body);
  void commandJson(const QByteArray &json, const QString &expectedId = QString());
  void runCommand(const QJsonObject &command);
  void publishCrashReports();
  bool dispatch(const char *event, const char *marker, const QJsonObject &detail,
                Delivery delivery = Delivery::Drop);
  void signInIfSignedOut();
  const QString origin_;
  QPointer<QCefWidget> browser_;
  QCefCookieManager *cookieManager_ = nullptr;
  QTimer signInTimer_;
  QElapsedTimer lastSignIn_;
  QString lastHost_;
  bool background_ = false;
  QTimer timer_;
  QTimer stateTimer_;
  QTimer cookieFlushTimer_;
  QTimer healthTimer_;
  QTimer layoutTimer_;
  QTimer heldTimer_;
  ScriptGate gate_;
  // Set by the first urlChanged: from then on the gate follows the page's own loads.
  bool pageSeen_ = false;
  // The browser has had a show event, real or HiddenStart's, so obs-browser has created it.
  bool started_ = false;
  // A dock closed since OBS started still gets its page (hidden-start.hpp).
  void startIfHidden();
  QList<QByteArray> held_;
  quint64 dropped_ = 0;
  QByteArray lastLayout_;
  int layoutQuiet_ = 0;
  quint64 layoutSequence_ = 0;
  AudioTap audio_;
  QString sessionId_;
  QString selectedUuid_;
  quint64 sequence_ = 0;
  quint64 stateSequence_ = 0;
  quint64 healthSequence_ = 0;
  QString lastCommandId_;
  QString partsId_;
  int partsCount_ = 0;
  QStringList parts_;
  QJsonArray crashReports_;
  bool crashReportsRead_ = false;
  int crashDeliveries_ = 0;
  bool trustedPage_ = false;
  bool sends_ = true;
  bool obeys_ = true;
  bool shutdownPrepared_ = false;
};
