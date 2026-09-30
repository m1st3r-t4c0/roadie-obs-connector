#pragma once
#include "audio-tap.hpp"
#include <QPointer>
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
  explicit SignalDock(QCef *cef, QWidget *parent = nullptr);
  ~SignalDock() override;
  bool initialized() const;
  void chooseMicrophone();

private slots:
  void navigationChanged(const QString &url);

private:
  void publish();
  void publishState();
  void saveSelection();
  QPointer<QCefWidget> browser_;
  std::unique_ptr<QCefCookieManager> cookieManager_;
  QTimer timer_;
  QTimer stateTimer_;
  QTimer cookieFlushTimer_;
  AudioTap audio_;
  QString sessionId_;
  QString selectedUuid_;
  quint64 sequence_ = 0;
  quint64 stateSequence_ = 0;
  unsigned stateTicks_ = 0;
  QJsonObject lastState_;
  bool trustedPage_ = false;
};
