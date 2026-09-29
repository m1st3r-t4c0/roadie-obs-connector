# Roadie OBS connector

This repository owns the native OBS Signal dock wrapper, Windows build tooling
and release packaging. The hosted `/dock` UI and web-side audio receiver remain
in `m1st3r-t4c0/roadie`; their product requirements live in that repository's
`docs/specs/signal-activity.md`.

Work in the selected Local checkout. Keep credentials and machine-specific paths
out of tracked files. Do not read or write Claude-owned instruction/configuration
files. Preserve changes made by others and commit only your own work.

Local commits are automatic. Show the commits and diff and obtain explicit user
approval before pushing, publishing releases, opening pull requests, or installing
into OBS. Never force-push. Do not update dependencies without package-specific
approval. Native tool preparation targets Windows x64 / minimum OBS 30.0.0 and Qt 6.5.3.
The user requires support for older OBS without updating their streaming machine.
Use baseline SDKs; verify newer versions and the browser panel ABI before claiming
compatibility. Other platforms require separate verification.

OBS binaries, SDK archives and compiler outputs belong in ignored `build/` and
`stage/` directories. Check downloaded SDK/tool hashes against official metadata.
Include matching upstream browser-panel declarations; do not recreate CEF vtables.
The audio bridge stays local and one-way; no raw audio or credentials enter it.

The user will perform OBS host testing. A successful compilation does not establish
host compatibility, sign-in persistence or microphone behavior. State unverified
checks honestly in the build documentation.