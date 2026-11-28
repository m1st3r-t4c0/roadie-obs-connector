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

// The document title the dock page (and the plugin's sign-in step) writes just before the page
// navigates away. Shared with roadie's apps/web/src/dock/native/obsLeaving.ts.
inline constexpr char leavingTitle[] = "roadie-obs-leaving";

class ScriptGate {
public:
  // A load that never reports a title (CEF may skip OnTitleChange when a reload keeps the same
  // title) opens the gate this long after the URL changed.
  static constexpr qint64 loadFallbackMs = 3000;

  // The page started a new load: urlChanged, or the plugin called reloadPage/setURL.
  void loading()
  {
    loading_ = true;
    leaving_ = false; // a new page committed: its frame exists, the load rules apply
    sinceLoad_.start();
  }
  // The page reported its title, so its document (and main frame) exists again.
  // `command` is a title the page writes to talk to the plugin. While leaving, that is the old
  // document, which keeps running until the commit, so it does not reopen the gate. Any other
  // title is a document's own: after leavingTitle it can only be the next page (a reload of the
  // same URL brings no urlChanged), or the same page carrying on after a cancelled navigation.
  void titleSeen(bool command = false)
  {
    loading_ = false;
    if (!command)
      leaving_ = false;
  }
  // #412: the page is about to leave. The dock page writes leavingTitle from beforeunload and
  // pagehide, and the plugin's own sign-in step writes it just before it follows the Twitch link.
  // From then until the next page commits, CEF can be without a main frame, and urlChanged comes
  // only at that commit. A sign-in redirect can take longer than loadFallbackMs to commit, so the
  // time fallback does not apply here: the gate follows the next urlChanged's rules, or opens
  // after leavingFallbackMs (a navigation the page started and then cancelled, such as a download).
  static constexpr qint64 leavingFallbackMs = 15000;
  void leaving()
  {
    leaving_ = true;
    sinceLeaving_.start();
  }
  // The page is closing or closed. Never opens again.
  void close() { closed_ = true; }

  bool closed() const { return closed_; }
  bool isLeaving() const { return leaving_ && sinceLeaving_.elapsed() < leavingFallbackMs; }
  bool pageReady() const
  {
    if (isLeaving())
      return false;
    return !loading_ || sinceLoad_.elapsed() >= loadFallbackMs;
  }
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
  bool leaving_ = false;
  bool closed_ = false;
  QElapsedTimer sinceLoad_;
  QElapsedTimer sinceLeaving_;
};
