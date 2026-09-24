# Changelog

All notable changes follow Semantic Versioning.

## [0.1.8-alpha] - 2026-09-24

### Display
- Every full page transition now performs a quality white anti-ghost cleanup before the new buffered frame is committed.
- In-page interactions continue to use regional refreshes so calculator keys, Focus, Fun, OTP and scan results do not trigger unnecessary full cleans.
- Buffered rendering remains enabled, so the new page is still composed off-screen and appears in one pass after the cleanup.

### Wi-Fi
- Wi-Fi Analyzer now shows nearby SSID, RSSI, channel and security state.
- Scan results are selectable on the M5Paper.
- Open networks connect immediately.
- Protected networks open the new native touch keyboard for password entry and are saved for automatic reconnect.
- Successful Wi-Fi connections synchronize system time from NTP and update the hardware RTC.
- At boot, PaperOS seeds Unix time from the RTC so time-dependent tools can continue to work offline.

### Bluetooth
- BLE Inspector now keeps scan results and opens a detailed advertising-data view.
- Device detail includes address, RSSI, advertised TX power, service UUID and manufacturer bytes when available.
- Inspector remains passive and does not pair, connect or write to nearby devices.

### Browser Lite
- Added a native HTTP/HTTPS text browser designed for ESP32/e-paper constraints.
- Supports URL entry, redirects, title/text extraction and a small list of absolute links.
- JavaScript, video, downloads and complex CSS are intentionally unsupported.
- HTTPS currently uses lightweight transport without CA certificate validation and is labelled accordingly in the UI.

### OTP
- Added RFC6238 TOTP generation using HMAC-SHA1, six digits and a 30-second period.
- OTP Base32 secret is held only in volatile RAM and is not saved to LittleFS or microSD.
- OTP can use RTC-restored system time offline after the clock has previously been synchronized.

### Input
- Added a reusable on-screen touch keyboard with lower/upper case, numeric keys and common symbols for Wi-Fi passwords, URLs and OTP secrets.

## [0.1.7-alpha] - 2026-09-24

### Buffered one-pass UI
- Disabled M5GFX framebuffer auto-display on the original M5Paper.
- PaperOS now composes complete screens off-screen before the explicit e-paper refresh, so cards, text and icons appear together instead of drawing visibly one element at a time.
- Notes and Files now compose their complete SD-backed page off-screen and perform a single physical refresh; no unnecessary intermediate loading refresh is used.
- Loading/progress screens are reserved for genuinely slow operations such as Wi-Fi/Bluetooth scans.
- Partial refresh remains enabled only for genuinely dynamic regions such as calculator results, battery data, clock, focus timer, Wi-Fi results and random-tool results.

### Added
- Clock: large desk-clock screen with date, battery and one-minute regional refresh.
- Focus: lightweight 25-minute concentration timer that keeps running while other PaperOS apps are open.
- Fun: D6 dice, coin toss and random number 1–100 using the ESP32 hardware random source.

### Performance
- Kept `epd_fastest` for interactive page commits.
- Full-screen construction no longer triggers automatic e-paper updates for every draw operation.
- New utility apps are designed around infrequent or regional e-paper refreshes.

## [0.1.6-alpha] - 2026-09-24

### Performance and UI
- Normal interactive page changes now use M5GFX `epd_fastest` for the original M5Paper.
- Notes and Files open their complete app shell immediately, show a loading/progress state, then replace only the content region after microSD enumeration.
- Wi-Fi keeps the full page responsive while asynchronous scanning runs and shows a visible loading bar.
- Bluetooth shows a dedicated scan/loading state and updates the device list with a regional refresh.
- Battery status in the top bar is now a direct shortcut to the Power Center.

### Battery / Power Center
- Added a native professional Battery / Power Center app.
- Added battery percentage, voltage, nominal 1150 mAh capacity, voltage trend, wake reason and sleep configuration.
- Added a clearly-labelled `chargingLikely` estimate based on sustained voltage rise.
- Added a Web UI Power Center and richer `/api/battery` telemetry.
- Current in mA is deliberately shown as unavailable because original M5Paper hardware has no battery-current or charger-state monitor.
- Added the official 5 V / 500 mA input specification as a hardware reference, not as a measured charge current.

### Sleep / Wake
- Fixed indefinite deep sleep: PaperOS previously passed `0` to M5Unified, which means "do not enter deep sleep".
- Indefinite sleep now passes the no-timer sentinel and uses the original M5Paper GT911 wake pin through M5Unified.
- Prevented an indefinite sleep configuration with no wake source by forcing touch wake when no timer is configured.
- Sleep screen now reports battery voltage and gives explicit touch-wake instructions.

## [0.1.5-alpha] - 2026-09-23

### Changed
- Reworked the native 540×960 GUI for a denser, more professional e-paper PDA layout.
- Home now uses a compact device overview, quick-access cards, module shortcuts and a cleaner web-console panel.
- App tiles now use stronger monochrome icon treatment and clearer READY/SOON states.
- Settings now uses grouped mobile-style rows with less visual clutter.
- Normal page navigation now uses the faster e-paper waveform instead of text-mode full-screen refresh.
- Automatic anti-ghost full cleans are much less frequent; manual Clean Display remains available.

### Performance
- Removed a redundant full-panel refresh during startup and shortened the splash hold delay.
- Wi-Fi scanning is now asynchronous, so opening the Wi-Fi app no longer blocks the interface.
- Bluetooth opens immediately and scans only when the user taps SCAN.
- Calculator key presses now refresh only the result region instead of redrawing the full keypad.
- Dynamic Wi-Fi/BLE result lists use regional e-paper refreshes.

## [0.1.4-alpha] - 2026-09-23

### Added
- iPhone-inspired grouped Settings screen with real navigation rows.
- Generali/Info screen with device model, ESP32 processor, PSRAM, flash, 540×960 display, firmware and WinLabs Solutions attribution.
- Native Wi-Fi app with live connection data and nearby network scanning.
- Native Bluetooth LE scanner app with nearby device name/address and RSSI.
- Native Calculator utility with touch keypad and basic arithmetic.
- Labs app branded WinLabs Solutions for experimental features.
- USB HID / BadUSB Lab script-library page backed by /PaperOS/Labs/HID.
- Labs storage directories for HID and experimental scripts.

### Changed
- Main app launcher expanded to include Calculator, Wi-Fi, Bluetooth, Labs and Reader.
- Bluetooth status icon now reflects the native BLE scanner state.
- Settings layout now uses grouped rounded cards and icon rows inspired by modern mobile settings UIs.

### Hardware note
- Original M5Paper USB-C is connected through CP2104/CH9102 USB-to-serial hardware, so it cannot natively enumerate as a USB HID keyboard. HID script execution therefore requires an external HID-capable adapter; PaperOS does not fake unsupported USB execution.


## [0.1.3-alpha] - 2026-09-23

### Changed
- Locked the on-device design system to the original M5Paper's native 540×960 portrait resolution.
- Redesigned Home to match the PaperOS concept more closely: status bar, dashboard cards, bottom navigation and high-contrast spacing tuned for the real panel.
- Solar and Termo now have dedicated finished UI shells but remain explicitly Coming Soon with no fake telemetry.
- Anti-ghost cleanup now performs a high-quality white refresh before redrawing the active screen.
- Auto sleep now stays asleep until touch/button wake instead of waking again after the inactivity timeout.

### Added
- Dedicated Solar planned dashboard with PV, home, battery, grid and MPPT placeholders.
- Dedicated Termo planned dashboard with room, humidity, puffer and boiler integration placeholders.
- Retained e-paper sleep screen with clock, battery, wake instructions and optional timer information.
- Manual Sleep Now and Sleep 15 min actions.
- Persistent touch-wake and optional timed-wake settings exposed through the browser console.
- Browser navigation entry for the Termo module.

### Fixed
- Corrected the power-management model so inactivity sleep and scheduled wake are separate behaviors.
- Realigned touch hitboxes to the redesigned 540×960 page geometry.


## [0.1.2-alpha] - 2026-09-23

### Fixed
- Reworked the original M5Paper refresh policy to reduce visible ghosting and long page-change delays.
- Removed fractional text scaling and faint gray UI text that rendered poorly on the real e-paper panel.
- Added explicit invalid-RTC handling in the status bar.

### Changed
- Normal full-page navigation now uses `epd_text` instead of the slow quality mode.
- PaperOS uses a high-contrast 1-bit black/white drawing path and bold M5GFX fonts.
- Automatic white-screen anti-ghosting cleanup runs after several page transitions, with an additional periodic cleanup.
- Status-only updates use a faster regional refresh without forcing a slow full-screen cycle.

### Added
- Native Notes browser on the M5Paper, including direct note reading from microSD.
- Native Files browser with directory navigation and previews for TXT, Markdown, JSON, LOG and CSV files.
- Native Tools page with Clean Display, System Monitor, Settings, Sleep and browser-console access.
- Manual Clean Display action for immediate ghosting removal.
- More native navigation targets in the App Launcher.


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
