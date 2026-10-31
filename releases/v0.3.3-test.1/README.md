# Roadie Signal 0.3.3 for Windows

Tag: `v0.3.3-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3. The first release with a
**macOS (Apple Silicon)** build, thanks to **CtrlGremlin**, who ported the plugin to the Mac.

The tag retains its original test suffix. See Verification below for the recorded build checks.

## What is new

- **Roadie Signal on the Mac.** CtrlGremlin ported the plugin to Apple Silicon Macs. The published Mac package is a separate 0.3.1 release with its own install guide, not a Windows DLL.
- **No crash when quitting OBS on a Mac.** CtrlGremlin also found and fixed three crashes that hit
  every quit with the dock open. This fix is macOS only.
- **Windows is unchanged.** The Windows DLL differs from 0.3.2 only in its version number, so a
  Windows user on 0.3.2 does not need this update.

Everything from 0.3.2 is unchanged, including staying signed in on OBS 33.

## Install on Windows

The OBS connector is the recommended way to connect Roadie to your stream. Use the Firebot
plugin only if you plan to connect Roadie's queue system to Firebot.

1. **Download** from [roadie-obs-connector on GitHub](https://github.com/m1st3r-t4c0/roadie-obs-connector).
   Choose Windows in the Download table, then download the Windows ZIP under the release's
   **Assets**. Do not choose **Source code**. The ZIP contains `roadie-signal.dll` inside
   `obs-plugins/64bit/`; the same DLL is also offered as a standalone asset. No compilation is needed.
2. **Install on your machine.** Check your OBS version in **Help → About**, close OBS completely,
   extract the ZIP, and copy **only `roadie-signal.dll`** to the destination below. Create the
   destination folder if it does not exist. Windows may ask for administrator permission.
3. **Restart OBS.**
4. **Open the dock:** choose **Docks → Roadie Signal**.
5. **Log in** to Roadie inside the dock.

### OBS before v33 (OBS 30–32)

Put the DLL in `obs-plugins\64bit` under the OBS installation you actually run. The normal path is:

```text
C:\Program Files\obs-studio\obs-plugins\64bit\roadie-signal.dll
```

The Windows plugin requires OBS 30.0.0 or newer; OBS 29 and earlier are not supported.

### OBS v33 and newer

Put the DLL directly in its own ProgramData folder:

```text
C:\ProgramData\obs-studio\plugins\roadie-signal\roadie-signal.dll
```

Paste `C:\ProgramData\obs-studio\plugins` into File Explorer's address bar if ProgramData is
hidden. Create `roadie-signal`, then put the DLL inside it. Do **not** nest the ZIP's
`obs-plugins\64bit` folders here, and do not put the DLL in OBS's `core` folder.

When moving an existing install, close OBS and remove only the old Roadie DLL from
`obs-plugins\64bit` (or the old ProgramData `roadie-signal\bin\64bit` folder), including renamed
copies. Keep one installed Roadie DLL, with its original name. Keep your OBS settings and scenes.
The old Program Files location is flagged Legacy in OBS 33 and will not load in OBS 34.
See [OBS's plugin location guidance](https://obsproject.com/kb/legacy-plugin-developer-info).

### Portable OBS

Use the OBS folder you launch in portable mode:

- **OBS 30–32:** `<portable OBS>\obs-plugins\64bit\roadie-signal.dll`.
- **OBS 33+:** `<portable OBS>\plugins\roadie-signal\roadie-signal.dll`.

Portable OBS ignores ProgramData plugins. Do not install in ProgramData for a portable launch.

### After login and troubleshooting

The channel owner turns on **Share OBS state with Roadie** in Signal's gear. Open
**Connectors → OBS** in Roadie and switch a scene in OBS to check that the scene updates.
OBS WebSocket is not required. **Tools → Roadie Signal…** shows the loaded version and dock
status; it also controls which Roadie environment receives OBS data and may change OBS.

If the dock is missing, check the OBS version, x64 package, destination folder and loaded DLL
path in the OBS log. Close and reopen OBS after copying the file. Do not add a Custom Browser
Dock as an installation step: the plugin registers its own dock. To update, close OBS and replace
that DLL. To uninstall, close OBS and remove that DLL. The Windows build is unsigned.

## Install on a Mac

Download the [separate macOS release](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/macos-v0.3.1-test.1), open its ZIP, and follow its `README.txt`: quit OBS, right-click
`install.command`, choose **Open**. It needs an Apple Silicon Mac and the OBS version named in
the package's `build-report.json`, or newer. The plugin is ad-hoc signed, not notarized, so macOS
warns before the first run.

## Verification

Windows: built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean source tree. The DLL's
imports resolved against OBS 30.0.0 and OBS 32.0.4. These are dependency checks, not an OBS host
run. `build-report.json` records the source commit and DLL hash.

macOS: the separate 0.3.1 release records its own source, patches, verification and known limits. It is not the Windows 0.3.3 DLL. See that release before installing.
