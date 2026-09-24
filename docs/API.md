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


## Battery / Power Center

`GET /api/battery` returns the original M5Paper battery telemetry used by the native and Web Power Center:

- `percent` — M5Unified battery-level estimate.
- `millivolts` — measured battery voltage.
- `nominalCapacityMah` — 1150 mAh nominal pack capacity.
- `trend` / `trendDeltaMv` — sampled battery-voltage direction.
- `chargingLikely` — heuristic only; true when a sustained voltage rise suggests charging.
- `currentSupported` — false on original M5Paper.
- `currentMa` — null because the board has no battery-current monitor.
- `wakeReason` — ESP32 wake source reported after boot.

The original M5Paper cannot report real charger state or battery current. PaperOS deliberately keeps current unavailable instead of fabricating a value.
