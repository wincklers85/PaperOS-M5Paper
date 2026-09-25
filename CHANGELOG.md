# Changelog

All notable changes follow Semantic Versioning.

## [Unreleased] — stabilization work toward 0.3.2-alpha

### Build and validation
- CI now checks Web UI JavaScript syntax alongside repository preflight, firmware compilation and LittleFS packaging.
- Fixed the SD Recovery source code to compile with the classic ESP32 C++ toolchain, based on the compile log from the target build.
- Updated build, release, security and device-test documentation to reflect the 0.3.1-alpha codebase and the 0.3.2-alpha stabilization gate.
- Corrected SD tests for the no-write-on-boot behavior and documented the recovery-format and same-card-save limits.
- System information now distinguishes directly usable PSRAM from the 8 MB physically installed on the original M5Paper.

## [0.3.1-alpha] - 2026-09-25

### Lock / standby
- User-facing Standby is now a soft lock instead of ESP32 deep sleep.
- Wi-Fi, Bluetooth HID and Phone Link remain alive while locked.
- Lock can be triggered from the pull-down Quick Settings panel.
- Mouse-wheel click or the M5Paper center button arms unlock.
- A right swipe on the touchscreen completes unlock and opens the Apps menu.
- Automatic inactivity timeout now requests the same soft lock.
- Real hardware deep sleep remains available explicitly through the ROOT Terminal with `deepsleep`.

### Mouse wheel navigation
- BLE mouse wheel up/down now navigates PaperOS menus and scrollable pages.
- Menu items are visibly outlined while wheel navigation is active.
- Pressing the wheel acts as OK/Enter.
- Supported focus navigation includes Home, Apps, Settings, Files, Tools, Network Toolkit, Labs, Wi-Fi, BLE result lists and Classic Desktop.
- Up/Down/Enter/Esc from a BLE keyboard mirror the same menu navigation behavior.

### Notification Center
- Added a bottom-up Notification Center opened by swiping upward from the bottom edge.
- Swipe downward closes it.
- Shows iPhone/Phone Link connection state, ANCS state and recent notifications.
- Incoming calls, missed calls, mail, social, calendar and other ANCS categories are labelled when available.
- Mouse wheel scrolls the notification list.
- Tapping the phone card or a notification opens Phone Link.

### iPhone battery status
- ANCS itself does not expose iPhone battery percentage, so PaperOS does not fabricate it.
- Added companion/Shortcut phone status support for real battery percentage and charging state.
- Custom BLE status payload accepts `type=status`, `battery`, `charging` and `device`.
- REST endpoint `POST /api/phone/status` accepts the same data.
- `GET /api/phone/status` now reports phone name, battery, charging state and status age.
- Notification Center displays `N/D` until a companion or Shortcut has supplied battery status.

### SD Recovery
- Added best-effort FAT32 deleted-entry scanning, previews for supported images/text and authenticated browser export.
- Added optional same-card save with two confirmations, full-file PSRAM staging and a 1 MiB limit; other deleted data can still be overwritten.
- NTFS and APFS recovery remain unsupported.

## [0.3.0-alpha] - 2026-09-24

### Classic Desktop / Windows 3.11 mode
- Added a full-screen Classic Desktop app launched from the Apps page.
- Added a vector-rendered Windows 3.11-style startup splash followed by Program Manager.
- Program Manager launches ROOT Terminal, File Manager, Control Panel, Bluetooth Input, Ski, Solitaire, Network and I/O tools.
- Touchscreen and BLE mouse share the same pointer/click path.
- BLE keyboard input is routed directly into Classic Desktop apps.
- Esc and Alt+F4 return from Classic windows.

### Bluetooth mouse / keyboard
- Added BLE HID Host support for devices advertising standard HID service UUID 0x1812.
- Scans and identifies likely keyboards/mice, connects and subscribes to Boot Keyboard, Boot Mouse and input Report characteristics.
- Supports pointer movement, primary click, keyboard characters, arrows and common control keys.
- Multiple BLE HID clients can remain connected simultaneously when memory/radio conditions allow.
- This release targets BLE HID; Bluetooth Classic HID is not enabled yet.

### ROOT Terminal
- Added a real PaperOS command console instead of a simulated DOS prompt.
- Filesystem: pwd, cd, dir/ls, type/cat, mkdir/md, del/rm, copy and move.
- Hardware/system: sysinfo, heap, battery, time, sd, reboot and sleep.
- GPIO: list, read, mode, write and ADC commands.
- Port A/B/C GPIO work without unlocking. `unsafe on` permits raw access to GPIO 0..39 and warns that internal display/SD pins can be disrupted.
- Network/radio: Wi-Fi status/on/off/scan and BLE HID status/scan/connect.
- NFC tag scan is available directly from the terminal.
- Classic drive aliases: `C:` maps to `/PaperOS`; `D:` maps to microSD root `/`.

### Games
- Added Ski with keyboard arrows/A-D, touch/mouse steering, obstacles, score and timed movement.
- Added Klondike Solitaire with 52-card shuffle, stock, waste, seven tableau piles and four suit foundations.
- Solitaire currently moves one exposed card at a time, with alternating-color tableau and ascending-suit foundation rules.

### File Manager
- Fixed SD visibility: the native File Manager can now switch directly between `/PaperOS` and the complete microSD root `/`.
- Added selection mode and real Cut / Copy / Paste controls.
- Copy works recursively for folders.
- Cut uses move/rename when possible and falls back to copy/remove.
- Paste creates a duplicate-safe destination name instead of silently overwriting an existing item.
- Folder deletion is recursive while `/` and `/PaperOS` roots remain protected.

## [0.2.0-alpha] - 2026-09-24

### iPhone Link / Apple ANCS
- Added native Apple Notification Center Service (ANCS) accessory mode on the original ESP32 M5Paper.
- PaperOS advertises ANCS service solicitation, requests secure BLE bonding and connects back to the paired iPhone ANCS service.
- Added Notification Source / Data Source subscriptions and Control Point attribute requests for app identifier, title, message and action labels.
- iOS notification categories such as incoming call, missed call, social, email and calendar are shown natively on PaperOS.
- Added ANCS positive/negative notification actions when iOS advertises those actions.
- The existing custom BLE/REST companion bridge remains available for extra phone-controlled actions such as camera shutter, media controls and app-specific call/message workflows.
- Full Contacts, SMS history and call-history databases are not exposed by ANCS and are not falsely presented as native data sources.
- Added first-pair instructions using nRF Connect as a reliable BLE discovery/pairing fallback for DIY accessories.

### Professional UI redesign
- Added persistent UI themes: Soft, Classic, Technical and Minimal.
- Soft is the new default with larger rounded corners and a cleaner mobile-style layout.
- Redesigned Settings icons as drawn e-paper symbols instead of plain abbreviation blocks.
- Redesigned Wi-Fi, Bluetooth, microSD and battery status icons.
- Quick Settings now behaves as a floating pull-down sheet above the existing framebuffer.
- Added a dithered/frosted background layer and panel shadow to simulate blur and 3D depth on the monochrome e-paper display.
- Rounded icon buttons are used for Quick Settings and iPhone Link controls.

### NFC Lab
- Added optional PN532 support through the Seeed PN532 library.
- NFC Lab uses HSU/UART on original M5Paper Port C: G18 RX and G19 TX.
- Added PN532 firmware/module test and ISO14443A passive tag scanning with UID display.
- NFC Lab is read-only in this release: no tag writes or card emulation.
- UI documents that Port C red supply is 5 V and module power must match the specific PN532 board rating.

## [0.1.9-alpha] - 2026-09-24

### Navigation / gestures
- Added a real Back arrow with page history, plus left-edge swipe-right navigation.
- Added pull-down Quick Settings; swipe down from the status area to open and swipe up to close.
- Quick Settings provides Standby, Wi-Fi radio toggle, Bluetooth toggle, EPD quality profile, NTP time sync and Battery shortcut.
- Added real swipe scrolling to Settings, Files, Wi-Fi results, BLE results, Phone Link notifications, Web Reader text/links and long text/PDF previews.

### E-paper / battery
- Added Fast / Balanced / Clean EPD quality profiles instead of pretending the e-paper has LCD-style analog contrast.
- Battery Power Center now graphs real rolling battery-voltage samples.
- Added a clearly labelled estimated subsystem-impact ranking for Wi-Fi, BLE, e-paper refresh and active UI.
- Battery-current mA remains unavailable on original M5Paper hardware.

### Wi-Fi / network
- Saved networks are mirrored to editable `/PaperOS/Config/wifi_networks.txt` using SSID / Password / Priority blocks; PaperOS imports the file at boot.
- The Wi-Fi text file explicitly warns that passwords are plaintext by user choice.
- Added safe Wi-Fi Audit: security mode, open/legacy/modern counts, hidden SSIDs, signal levels and channel congestion.
- LAN Discovery now covers the full local /24 in four bounded ranges, with common-service probing similar to a lightweight Fing workflow.
- No deauth, password cracking, credential capture or access-bypass functions are included.

### Bluetooth / Phone Link
- BLE scan stores up to 40 devices and is scrollable.
- Added Bluetooth SIG company-ID identification for common manufacturers plus numeric fallback.
- BLE device test now performs a connection test, service discovery and read-only GATT characteristic reads.
- Added Phone Link BLE companion bridge with notification input and command READ/NOTIFY characteristic.
- Added companion commands for camera shutter, media control, phone ring, calls and messages; execution requires a permitted phone companion.
- Added authenticated REST bridge endpoints for companion/test workflows.

### Web Reader
- Added non-JavaScript DuckDuckGo HTML search as the default text search.
- Added relative/internal URL resolution so site navigation is more useful.
- Added scrollable text and link windows while retaining the ESP32 reader-mode limits.

### Files / microSD
- File Manager now caches up to 160 directory entries and scrolls them.
- Added scrollable readers for TXT, Markdown, JSON, LOG, CSV, INI, CFG, XML, HTML and YAML.
- Added JPEG, PNG and BMP decoding from microSD through M5GFX.
- Added PDF Reader Lite with basic extraction of uncompressed PDF text; complex/compressed PDFs are explicitly reported as unsupported for full rendering.
- Added settings backup/restore to `/PaperOS/Backup/settings.json`.
- Added confirmed destructive SD formatting using FAT or exFAT, plus auto format selection. Formatting creates one whole-card PaperOS volume.
- Multi-partition editing remains disabled because the current firmware mount layer exposes one SD volume.

### Date / time
- Added dedicated Date & Time settings.
- Added one-tap NTP resynchronization and manual RTC/system-time entry.
- NTP synchronization continues to update the hardware RTC so TOTP can work offline afterward.

### GPIO
- Added GPIO Lab for original M5Paper Port A (G25/G32), Port B (G26/G33) and Port C (G18/G19).
- All six signals initialize as INPUT; users explicitly cycle a pin to OUTPUT LOW/HIGH.
- UI warns that signal pins are 3.3 V logic and the Grove red supply is 5 V.

### Input
- Keyboard keys now visibly invert when pressed.
- Password fields have SHOW/HIDE control.

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
- Advertising scan remains passive; GATT connection occurs only when the user explicitly taps READ GATT, and the reader performs no writes.

### Web Reader
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

### Network Toolkit
- Added real ICMP Ping with average latency.
- Added DNS hostname lookup using the active Wi-Fi resolver.
- Added Quick LAN Service Scan for addresses .1-.64 and common TCP ports 22, 80, 443, 1883 and 8080 with short timeouts.
- Added HTTP/API Tester with GET, POST and PUT plus response status/body.
- Added a persistent local MQTT client with broker connection, subscribe, publish and last-message display.
- Added Wake-on-LAN magic packet sender using local subnet broadcast.
- Added PlatformIO dependencies PubSubClient 2.8 and ESP32Ping 1.7.

### BLE tools
- BLE Inspector now classifies iBeacon and Eddystone advertisements when their standard markers are present.
- Added generic beacon/manufacturer advertising identification.
- Added GATT Reader: explicitly connects to the selected device and reads characteristics that advertise the READ property.
- GATT Reader is read-only in this alpha; no characteristic writes are performed.

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
