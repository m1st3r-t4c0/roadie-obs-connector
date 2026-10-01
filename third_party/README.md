# obs-websocket API

`obs-websocket-api.h` is the unmodified upstream 5.3.0 public in-process API:
https://github.com/obsproject/obs-websocket/blob/5.3.0/lib/obs-websocket-api.h

Its GPL-2.0-or-later copyright/license notice is retained in the header.
The plugin calls `CallVendorRequest` for obs-browser's existing `emit_event`
endpoint, available with the OBS 30 baseline. No network WebSocket is opened.
