# PaperOS

**PaperOS 0.1.9-alpha** is an offline-first, e-paper-native PDA firmware for the **original M5Stack M5Paper (ESP32-D0WDQ6-V3)**. It is intentionally not an Android clone. The architecture is designed around limited RAM, PSRAM, low power, e-paper refresh constraints, microSD storage and browser-based administration.

> Target: M5Stack M5Paper first generation only. M5Paper S3 is not the target of this repository.

## What is real in 0.1.9-alpha

- M5Unified/M5GFX initialization for original M5Paper
- boot splash and touch UI
- pixel-precise 540×960 Home, Apps, Settings, System Monitor, native Notes, native Files and Tools screens
- hardware buttons: Home / Apps / hold power for sleep
- RTC display and battery telemetry
- buffered one-pass 540×960 rendering: screens are composed off-screen, then committed with `epd_fastest`; only live regions use partial refresh
- clean anti-ghost transition when navigating to another page, with buffered one-pass redraw and fast regional updates inside the same app
- dedicated Solar and Termo UI shells marked Coming Soon, with no fake telemetry
- corrected no-timer deep-sleep behavior with retained sleep screen, GT911 touch wake and optional timer wake
- native Wi-Fi Analyzer with asynchronous scan, RSSI/channel/security details, touch network selection, on-device password entry, saved networks, reconnect and Setup AP (`PaperOS-Setup`)
- native BLE Inspector with scrollable scans, manufacturer/company identification, beacon classification, connection testing and read-only GATT service/characteristic inspection
- native Calculator utility
- native Clock desk display, Focus 25-minute timer and Fun random tools (dice / coin / 1–100)
- native Web Reader with DuckDuckGo non-JavaScript text search, HTTP/HTTPS reader mode, relative-link navigation and scrolling
- native RFC6238 TOTP calculator with volatile-only Base32 secret storage
- reusable on-screen touch keyboard with pressed-key feedback and SHOW/HIDE for passwords
- native Battery / Power Center with real rolling voltage graph, wake diagnostics, charging-likely estimation and clearly-labelled subsystem impact estimate
- Web Power Center plus extended battery REST telemetry
- WinLabs Solutions Labs area with beta tools and HID script library
- captive DNS redirect to `192.168.4.1`
- mDNS hostname `paperos.local`
- password-protected responsive Web UI
- first-boot browser-assisted wizard
- microSD layout and scrollable file manager: list, upload, download, create folder, rename, copy and delete; native text, JPEG/PNG/BMP and PDF-Lite readers
- Notes on microSD with browser editing, categories, favorite and autosave
- settings persistence in LittleFS with atomic `.tmp` / `.bak` recovery
- system status REST API
- remote Home / Apps / refresh / sleep / reboot controls
- e-paper sleep screen retained during deep sleep
- timer/touch wake through M5Unified power management
- firmware OTA upload from the Web UI






## 0.1.9 interaction model

- **Back:** tap the arrow in the top-left status bar or swipe right from the left edge.
- **Quick Settings:** swipe down from the top; swipe up to close. It contains Standby, Wi-Fi, Bluetooth, EPD quality, NTP sync and Battery.
- **Scrolling:** swipe up/down in long Settings, Files, Wi-Fi, Bluetooth, Web Reader and Phone Link lists.
- **Editable Wi-Fi networks:** `/PaperOS/Config/wifi_networks.txt` is imported at boot. It intentionally stores passwords in plaintext because this workflow was explicitly requested; protect the SD accordingly.
- **Backups:** Settings can export/restore `/PaperOS/Backup/settings.json`.

## Network Toolkit

PaperOS includes a native network toolbox intended for diagnostics and local device work rather than general web browsing:

- **Ping** — real ICMP ping with average latency.
- **DNS Lookup** — resolves hostnames through the active Wi-Fi DNS server.
- **Quick LAN Service Scan** — probes `.1` through `.64` of the current IPv4 `/24` for common TCP services on ports 22, 80, 443, 1883 and 8080. It is intentionally bounded to keep the M5Paper responsive; it is a service scan, not a guaranteed inventory of every host.
- **HTTP/API Tester** — GET, POST and PUT with response code/body.
- **MQTT Client** — connect, subscribe, publish and show the latest message on a topic. The alpha client targets plain local MQTT on port 1883; TLS/authentication are not yet implemented.
- **Wake-on-LAN** — sends a standard magic packet to the current subnet broadcast address.

BLE Inspector also recognises common iBeacon/Eddystone advertising markers and offers a **read-only GATT Reader** for connectable BLE devices. Only characteristics advertising the READ property are read; PaperOS does not write characteristics from this tool.

## Web Reader and OTP

Web Reader is intentionally a reader-style browser rather than a desktop browser engine. It retrieves HTTP/HTTPS pages, follows redirects, extracts readable text and exposes a small number of absolute links. JavaScript, video, downloads and complex CSS are not executed. HTTPS currently uses a lightweight connection without CA certificate validation, and PaperOS says so in the interface.

The OTP tool implements standard RFC6238 TOTP with HMAC-SHA1, six digits and a 30-second step. The Base32 secret is kept only in RAM. PaperOS restores Unix time from the hardware RTC at boot and refreshes it from NTP whenever Wi-Fi connects, so OTP can continue working offline once the RTC is correct.

## Buffered e-paper rendering

M5GFX enables auto-display on framebuffer-backed panels. PaperOS explicitly disables that behavior on the original M5Paper so UI primitives are drawn into the framebuffer first and the physical e-paper panel is refreshed only when the complete screen is ready. This prevents the visible “one widget at a time” construction effect and makes navigation feel substantially more immediate.

## Battery current limitation on original M5Paper

The original M5Paper can report battery level and battery voltage, but its hardware does **not** expose real charger status or battery current to M5Unified. PaperOS therefore never invents a charge-current value. The Power Center shows current as **N/D**, displays the official 5 V / 500 mA input specification separately, and can only label charging as **probable** when a sustained battery-voltage rise is observed. Real charge/discharge current in mA requires an external current monitor such as INA219/INA226.

## Labs and USB HID note

The original M5Paper USB-C connector is wired through a CP2104/CH9102 USB-to-serial bridge, not a native USB HID controller. PaperOS therefore provides a **USB HID / BadUSB Lab** for script storage and future external-adapter workflows, but it does not pretend that the built-in USB-C port can execute HID scripts. Scripts are stored under `/PaperOS/Labs/HID`.

## Current limits

- **Solar and Termo telemetry** remain Coming Soon until real data sources are connected.
- **Phone Link** is a real BLE/REST bridge, but phone notifications, SMS/messages, calls and camera actions require a companion with Android/iOS permissions. See [PHONE_LINK.md](docs/PHONE_LINK.md).
- **PDF Reader Lite** extracts readable text from compatible/uncompressed PDF content; it is not a full graphical PDF engine.
- **Web Reader** does not execute JavaScript, video or complex CSS; HTTPS currently uses lightweight transport without CA validation.
- **SD formatting** supports one whole-card FAT/exFAT PaperOS volume. Multi-partition mounting/editing is intentionally not exposed yet.
- **Battery current in mA** is unavailable without an external current monitor.
- **Wi-Fi Audit** is diagnostic only: no deauth, credential capture, password cracking or access bypass.
- Solar/Termo controls, an on-device notes editor, reorderable Home widgets and a packaged phone companion app remain deferred.

See [ROADMAP.md](ROADMAP.md).

## Why M5Unified instead of M5EPD

M5Stack archived the original M5EPD library and now recommends M5GFX/M5Unified. M5Unified supports the original M5Paper and provides maintained APIs for display, touch, physical buttons, RTC and power management.

The PlatformIO project uses `esp32dev` as the base board because PlatformIO does not provide a dedicated original-M5Paper board manifest. It configures the real 16 MB flash / PSRAM layout and uses M5Unified with `board_M5Paper` as the fallback board. This keeps the build on classic ESP32 without PaperS3-specific APIs.

## Quick start

1. Install VS Code + PlatformIO.
2. Connect the M5Paper by USB.
3. Open this repository.
4. Build:

```bash
pio run -e m5paper
```

5. Flash firmware:

```bash
pio run -e m5paper -t upload
```

6. Flash the Web UI filesystem:

```bash
pio run -e m5paper -t uploadfs
```

7. Monitor serial output:

```bash
pio device monitor -b 115200
```

On first boot, join **PaperOS-Setup** and open **http://192.168.4.1**. Complete the wizard. If a Wi-Fi network is configured, PaperOS reboots and joins it; then use **http://paperos.local** on the same LAN.

See [BUILD.md](BUILD.md) for detailed build/flashing instructions.

## Repository layout

```text
PaperOS/
├── include/                 Shared models and constants
├── src/
│   ├── core/                Config + logging
│   ├── drivers/             display/touch facade
│   ├── network/             Wi-Fi/AP/mDNS
│   ├── services/            Notes + power
│   ├── storage/             microSD abstraction
│   ├── ui/                  on-device e-paper UI
│   └── web/                 HTTP server + REST API + OTA
├── data/                    LittleFS Web UI payload
├── www/                     Web UI notes
├── docs/                    API, hardware, security, tests
├── tests/
├── scripts/
├── partitions.csv
└── platformio.ini
```

## Storage layout

PaperOS creates:

```text
/PaperOS
  /Documents
  /Books
  /PDF
  /Notes
  /Images
  /Downloads
  /Logs
  /Backup
  /Config
```

All Web File Manager paths are sandboxed below `/PaperOS`.

## Release

Current semantic version: **0.1.9-alpha**.. See [CHANGELOG.md](CHANGELOG.md) and [RELEASE.md](RELEASE.md).

## License

MIT. See [LICENSE](LICENSE).
