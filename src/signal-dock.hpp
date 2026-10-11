#pragma once
#include "audio-tap.hpp"
#include <QPointer>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTimer>
#include <QWidget>
#include <QString>
#include <memory>

struct QCef;
class QCefWidget;
struct QCefCookieManager;

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
  // Whether this server receives OBS data, and whether its scene commands are followed.
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

signals:
  void pageChanged();

private slots:
  void navigationChanged(const QString &url);
  void titleChanged(const QString &title);

private:
  void publish();
  void runCommand(const QJsonObject &command);
  void publishCrashReports();
  void dispatch(const char *event, const char *marker, const QJsonObject &detail);
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
  AudioTap audio_;
  QString sessionId_;
  QString selectedUuid_;
  quint64 sequence_ = 0;
  quint64 stateSequence_ = 0;
  quint64 healthSequence_ = 0;
  QString lastCommandId_;
  QJsonArray crashReports_;
  bool crashReportsRead_ = false;
  int crashDeliveries_ = 0;
  bool trustedPage_ = false;
  bool sends_ = true;
  bool obeys_ = true;
  bool shutdownPrepared_ = false;
};
