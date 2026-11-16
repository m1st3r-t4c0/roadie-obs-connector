#pragma once
// #304: when it is safe to call QCefWidget::executeJavaScript on one dock page.
//
// obs-browser's executeJavaScript asks CEF for the page's main frame and uses it without checking,
// so a call at an instant the page has no frame crashes OBS (c0000005 in
// QCefWidgetInternal::executeJavaScript). That happens while a page reloads or changes URL, while a
// dialog or nested event loop runs (OBS's "dock closed" message box, #353), and once the page is
// closing. The gate stays shut in all three, on every platform; a call that finds it shut is
// dropped (or held, for a command result) instead of reaching CEF.
//
// Qt only, so tests/browser-probe drives the same gate against the real obs-browser.
#include <QApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QThread>

class ScriptGate {
public:
  // A load that never reports a title (CEF may skip OnTitleChange when a reload keeps the same
  // title) opens the gate this long after the URL changed.
  static constexpr qint64 loadFallbackMs = 3000;

  // The page started a new load: urlChanged, or the plugin called reloadPage/setURL.
  void loading()
  {
    loading_ = true;
    sinceLoad_.start();
  }
  // The page reported its title, so its document (and main frame) exists again.
  void titleSeen() { loading_ = false; }
  // The page is closing or closed. Never opens again.
  void close() { closed_ = true; }

  bool closed() const { return closed_; }
  bool pageReady() const { return !loading_ || sinceLoad_.elapsed() >= loadFallbackMs; }
  bool open() const { return !closed_ && pageReady() && !nestedOrModal(); }

  // In normal running the main thread sits at loop level 1; a dialog's exec(), a QMessageBox or a
  // menu runs a nested loop, and a timer that fires in it must not call into a page.
  static bool nestedOrModal()
  {
    return QThread::currentThread()->loopLevel() > 1 || QApplication::activeModalWidget() ||
           QGuiApplication::modalWindow();
  }

private:
  bool loading_ = false;
  bool closed_ = false;
  QElapsedTimer sinceLoad_;
};
