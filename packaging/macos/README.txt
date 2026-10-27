ROADIE SIGNAL - OBS PLUGIN FOR MAC
==================================

This is the Roadie Signal OBS plugin built for Apple Silicon Macs. It adds the
Roadie Signal dock to OBS and lets Roadie react to your scene changes.
build-report.json says which version this is and which OBS it was built
against.


WHAT YOU NEED
-------------
- A Mac with Apple Silicon (M1, M2, M3, M4 or newer). It will NOT work on an
  Intel Mac.
- OBS Studio at the version in build-report.json ("obsVersion") or newer.
  An older OBS will not load the plugin; the OBS log says why.
- A Roadie account (dashboard.roadie.gg).


HOW TO INSTALL
--------------
1. Quit OBS completely (OBS menu -> Quit OBS).

2. Install the plugin. Either:

   EASY WAY: Right-click "install.command" in this folder and choose Open.
   macOS will warn that it's from an unidentified developer. Click Open.
   A Terminal window shows what it did.

   (Double-clicking may be blocked the first time. Right-click -> Open
   gets around that. Or open Terminal, type "bash " (with a space), drag
   install.command into the window and press Return.)

   MANUAL WAY: Copy "roadie-signal.plugin" into this folder:
     ~/Library/Application Support/obs-studio/plugins/
   (In Finder: Go menu -> Go to Folder..., paste the path above. Create the
   "plugins" folder if it doesn't exist.) Then run this in Terminal so
   macOS doesn't block it:
     xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/roadie-signal.plugin

3. Start OBS.

4. Check it worked:
   - Tools menu -> "Roadie Signal..." should be there.
   - Docks menu -> "Roadie Signal" opens the dock. Sign in to Roadie in it.


SETTINGS
--------
Everything is in Tools -> Roadie Signal...: the microphone level source,
where OBS data is sent, and whose scene changes OBS follows.


TIPS FOR SMOOTH STREAMS
-----------------------
On the Roadie browser sources in your scenes (and other browser sources that
only appear in some scenes), open Properties and tick:
   "Shutdown source when not visible"
Leave "Refresh browser when scene becomes active" OFF.
Lots of browser sources running at once can make OBS miss frames on scene
switches. This setting fixes that.

Exception: keep Shutdown OFF on sources that must keep running in the
background, like alerts or a chat overlay that needs to keep its history.


HOW TO UNINSTALL / DISABLE
--------------------------
Quit OBS, then delete (or move out) this folder:
   ~/Library/Application Support/obs-studio/plugins/roadie-signal.plugin
Your scenes and Roadie browser sources are not affected.


NOTES
-----
- This is a test release.
- It's ad-hoc signed (not notarized by Apple), which is why macOS warns
  about it.
- Thanks to CtrlGremlin, who ported the plugin to the Mac, made the first
  Mac build and found the crashes on quit that this build fixes.
- License: GPL v2 (see LICENSE). Source:
  https://github.com/m1st3r-t4c0/roadie-obs-connector
  (build-report.json names the exact commit).
