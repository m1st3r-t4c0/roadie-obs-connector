#pragma once
#include "audio-tap.hpp"
#include <QPointer>
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
  void saveSelection();
  QPointer<QCefWidget> browser_;
  std::unique_ptr<QCefCookieManager> cookieManager_;
  QTimer timer_;
  AudioTap audio_;
  QString sessionId_;
  QString selectedUuid_;
  quint64 sequence_ = 0;
  bool trustedPage_ = false;
};
