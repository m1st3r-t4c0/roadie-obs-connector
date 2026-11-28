# Dock logic probe

Qt only, no OBS. Checks the script gate's leaving state (#412: no calls into a dock page between
"about to leave" and the next page's title, the 3 second load fallback included) and the crash
report's compaction (#394: header, crashed thread and module list first, inside 64 KB).

Configure with the same `CMAKE_PREFIX_PATH` as the plugin (Qt 6.5.3 from the pinned SDK), build
Release, and run `dock-logic-probe.exe` with Qt's `bin` first in PATH. Exit 0 is a pass.
