#pragma once
#include "audio-tap.hpp"
#include <QPointer>
#include <QJsonArray>
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
  // One dock per Roadie server. Each has its own origin and its own cookie store,
  // so signing in to staging never touches the production sign-in.
  SignalDock(QCef *cef, const QString &origin, const char *cookieStore, QWidget *parent = nullptr);
  ~SignalDock() override;
  bool initialized() const;
  void prepareForShutdown();
  void chooseMicrophone();
  // Re-reads the saved microphone, after another dock's chooser changed it.
  void reloadMicrophone();
  void publishState();
  void publishHealth();

private slots:
  void navigationChanged(const QString &url);
  void titleChanged(const QString &title);

private:
  void publish();
  void runCommand(const QJsonObject &command);
  void publishCrashReports();
  void dispatch(const char *event, const char *marker, const QJsonObject &detail);
  void saveSelection();
  const QString origin_;
  QPointer<QCefWidget> browser_;
  std::unique_ptr<QCefCookieManager> cookieManager_;
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
  bool shutdownPrepared_ = false;
};
