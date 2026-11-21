#pragma once
// A dock page that runs while its dock is closed.
//
// obs-browser creates a page's CEF browser only in QCefWidget's first showEvent. A Roadie Signal
// dock that is closed when OBS starts (unticked under Docks, or never placed) is never shown, so
// /dock never loads and Roadie gets no scene, stream or audio data until the streamer opens it.
// Once created, the browser keeps running when the dock is hidden; only the first show matters.
//
// So a page that has not been shown a moment after it was made is sent the show event OBS would
// have sent. That runs obs-browser's own start-up path (Init, then the window container), parented
// to the hidden dock, so nothing appears on screen. Opening the dock later shows the page that is
// already running; obs-browser's Init returns at once when the browser exists.
//
// Qt only, so tests/browser-probe drives the same call against the real obs-browser.
#include <QCoreApplication>
#include <QShowEvent>
#include <QWidget>

namespace HiddenStart {
// How long a page may wait for OBS to show its dock before it starts unseen. OBS restores dock
// visibility while it loads, so a dock that is open shows well inside this.
constexpr int delayMs = 2000;

inline void start(QWidget *browser)
{
  if (!browser)
    return;
  QShowEvent show;
  QCoreApplication::sendEvent(browser, &show);
}
} // namespace HiddenStart
