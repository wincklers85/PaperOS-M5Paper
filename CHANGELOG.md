# Changelog

All notable changes follow Semantic Versioning.

## [0.1.1-alpha] - 2026-09-23

### Changed
- Promoted the active PaperOS development baseline to 0.1.1-alpha.
- Defined the professional 540×960 e-paper GUI direction: reusable visual components, consistent spacing, status bar, dashboard cards and bottom navigation.
- Documented the UI design-system boundary so future screens share the same typography, geometry and refresh behavior.
- Kept the PlatformIO build version and firmware fallback version synchronized.

### Development workflow
- The GitHub repository is the source of truth for PaperOS; future implemented improvements should be committed there together with the appropriate version/changelog update.

## [0.1.0-alpha] - 2026-09-22

### Added
- Initial original-M5Paper PlatformIO target with 16 MB flash, PSRAM and dual OTA partitions.
- M5Unified/M5GFX display, GT911 touch, physical button, RTC and power integration.
- Boot splash, first-boot instructions, Home, Apps, Notes viewer, Settings and System Monitor.
- E-paper fast refresh for clock/status and periodic quality refresh.
- Wi-Fi stored networks, automatic reconnect, PaperOS-Setup AP, captive DNS and `paperos.local`.
- Responsive authenticated Web UI and first-boot browser wizard.
- REST endpoints for system, battery, Wi-Fi, files, notes, settings and remote device actions.
- microSD layout, recursive search, upload/download, mkdir, rename/move, copy and delete.
- Notes categories/favorites/search/autosave in Web UI.
- Atomic config recovery (`.tmp`/`.bak`) and atomic note writes.
- Rotating SD system log.
- Deep sleep with retained e-paper status screen and touch/timer wake.
- Firmware OTA upload with progress in the browser.
- Architecture, build, hardware, API, security, test and roadmap documentation.

### Known limitations
- On-device Notes editing is not yet available.
- Home widget layout is fixed in this alpha.
- OTA updates application firmware only; LittleFS Web UI is flashed separately.
- Tasks, Calendar, BLE, MQTT, Smart Home, Solar, GPIO, Serial, Automations, Notifications and readers are intentionally Coming Soon.
- Wi-Fi credentials are stored in device configuration; deployments needing stronger at-rest protection should enable ESP32 flash encryption in a later hardened profile.
