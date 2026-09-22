# PaperOS REST API — 0.1.0-alpha

All authenticated endpoints use the same-origin `paperos` HttpOnly session cookie. API token support is reserved for a later hardening release.

Implemented endpoints:

- `GET /api/setup/status`
- `POST /api/setup`
- `POST /api/login`
- `POST /api/logout`
- `GET /api/system/status`
- `GET /api/battery`
- `GET /api/wifi`
- `POST /api/wifi/connect`
- `GET /api/files?path=/PaperOS`
- `POST /api/files/mkdir`
- `POST /api/files/delete`
- `POST /api/files/rename`
- `POST /api/files/copy`
- `GET /api/files/download?path=...`
- `POST /api/files/upload?path=...`
- `GET /api/notes`
- `GET /api/notes/item?id=...`
- `POST /api/notes/item?id=...`
- `POST /api/notes/delete`
- `GET /api/settings`
- `POST /api/settings`
- `POST /api/device/home`
- `POST /api/device/apps`
- `POST /api/device/refresh`
- `POST /api/device/sleep`
- `POST /api/device/reboot`
- `POST /api/ota`
