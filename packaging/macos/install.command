#!/bin/bash
# Installs the Roadie Signal OBS plugin for the current user.
cd "$(dirname "$0")" || exit 1

PLUGIN="roadie-signal.plugin"
OBSDIR="$HOME/Library/Application Support/obs-studio"
DEST="$OBSDIR/plugins"
BACKUPS="$OBSDIR/plugins-backup"

finish() { echo; read -r -p "Press Return to close this window..."; exit "$1"; }

echo "Roadie Signal plugin installer"
echo "=============================="

if [ "$(uname -m)" != "arm64" ]; then
  echo "This build only works on Apple Silicon Macs (M1 or newer)."
  finish 1
fi

if pgrep -xq OBS; then
  echo "OBS is running. Please quit OBS first, then run this again."
  finish 1
fi

if [ ! -d "$PLUGIN" ]; then
  echo "Can't find $PLUGIN next to this installer."
  echo "Keep install.command and $PLUGIN in the same folder."
  finish 1
fi

mkdir -p "$DEST"

if [ -d "$DEST/$PLUGIN" ]; then
  mkdir -p "$BACKUPS"
  STAMP=$(date +%Y%m%d-%H%M%S)
  mv "$DEST/$PLUGIN" "$BACKUPS/$PLUGIN.$STAMP"
  echo "Backed up the existing plugin to:"
  echo "  $BACKUPS/$PLUGIN.$STAMP"
fi

cp -R "$PLUGIN" "$DEST/" || { echo "Copy failed."; finish 1; }
xattr -dr com.apple.quarantine "$DEST/$PLUGIN" 2>/dev/null

echo
echo "Installed to:"
echo "  $DEST/$PLUGIN"
echo
echo "Start OBS, then check Tools -> Roadie Signal... and Docks -> Roadie Signal."
finish 0
