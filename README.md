# PaperOS

**PaperOS 0.1.8-alpha** is an offline-first, e-paper-native PDA firmware for the **original M5Stack M5Paper (ESP32-D0WDQ6-V3)**. It is intentionally not an Android clone. The architecture is designed around limited RAM, PSRAM, low power, e-paper refresh constraints, microSD storage and browser-based administration.

> Target: M5Stack M5Paper first generation only. M5Paper S3 is not the target of this repository.

## What is real in 0.1.8-alpha

- M5Unified/M5GFX initialization for original M5Paper
- boot splash and touch UI
- pixel-precise 540×960 Home, Apps, Settings, System Monitor, native Notes, native Files and Tools screens
- hardware buttons: Home / Apps / hold power for sleep
- RTC display and battery telemetry
- buffered one-pass 540×960 rendering: screens are composed off-screen, then committed with `epd_fastest`; only live regions use partial refresh
- less frequent automatic anti-ghosting cleanup plus manual Clean Display
- dedicated Solar and Termo UI shells marked Coming Soon, with no fake telemetry
- corrected no-timer deep-sleep behavior with retained sleep screen, GT911 touch wake and optional timer wake
- native Wi-Fi Analyzer with asynchronous scan, RSSI/channel/security details, touch network selection, on-device password entry, saved networks, reconnect and Setup AP (`PaperOS-Setup`)
- native BLE Inspector with passive scanning, device detail, RSSI, address, TX power, service UUID and manufacturer advertising data
- native Calculator utility
- native Clock desk display, Focus 25-minute timer and Fun random tools (dice / coin / 1–100)\n- native Browser Lite for HTTP/HTTPS reader-mode browsing\n- native RFC6238 TOTP calculator with volatile-only Base32 secret storage\n- reusable on-screen touch keyboard for Wi-Fi, browser URLs and OTP input
- native Battery / Power Center with voltage trend, wake diagnostics and charging-likely estimation
- Web Power Center plus extended battery REST telemetry
- WinLabs Solutions Labs area with beta tools and HID script library
- captive DNS redirect to `192.168.4.1`
- mDNS hostname `paperos.local`
- password-protected responsive Web UI
- first-boot browser-assisted wizard
- microSD layout and file manager: list, upload, download, create folder, rename, copy and delete
- Notes on microSD with browser editing, categories, favorite and autosave
- settings persistence in LittleFS with atomic `.tmp` / `.bak` recovery
- system status REST API
- remote Home / Apps / refresh / sleep / reboot controls
- e-paper sleep screen retained during deep sleep
- timer/touch wake through M5Unified power management
- firmware OTA upload from the Web UI




## Browser Lite and OTP

Browser Lite is intentionally a reader-style browser rather than a desktop browser engine. It retrieves HTTP/HTTPS pages, follows redirects, extracts readable text and exposes a small number of absolute links. JavaScript, video, downloads and complex CSS are not executed. HTTPS currently uses a lightweight connection without CA certificate validation, and PaperOS says so in the interface.

The OTP tool implements standard RFC6238 TOTP with HMAC-SHA1, six digits and a 30-second step. The Base32 secret is kept only in RAM. PaperOS restores Unix time from the hardware RTC at boot and refreshes it from NTP whenever Wi-Fi connects, so OTP can continue working offline once the RTC is correct.

## Buffered e-paper rendering

M5GFX enables auto-display on framebuffer-backed panels. PaperOS explicitly disables that behavior on the original M5Paper so UI primitives are drawn into the framebuffer first and the physical e-paper panel is refreshed only when the complete screen is ready. This prevents the visible “one widget at a time” construction effect and makes navigation feel substantially more immediate.

## Battery current limitation on original M5Paper

The original M5Paper can report battery level and battery voltage, but its hardware does **not** expose real charger status or battery current to M5Unified. PaperOS therefore never invents a charge-current value. The Power Center shows current as **N/D**, displays the official 5 V / 500 mA input specification separately, and can only label charging as **probable** when a sustained battery-voltage rise is observed. Real charge/discharge current in mA requires an external current monitor such as INA219/INA226.

## Labs and USB HID note

The original M5Paper USB-C connector is wired through a CP2104/CH9102 USB-to-serial bridge, not a native USB HID controller. PaperOS therefore provides a **USB HID / BadUSB Lab** for script storage and future external-adapter workflows, but it does not pretend that the built-in USB-C port can execute HID scripts. Scripts are stored under `/PaperOS/Labs/HID`.

## Explicitly not implemented yet

The Web UI displays these modules as **Coming Soon** rather than simulating them: Tasks, Calendar, BLE Console, MQTT, Smart Home, GPIO Manager, Serial Terminal, Automations, Notifications, PDF Reader and Ebook Reader. Solar and Termo have finished UI shells but their telemetry backends remain Coming Soon. The on-device note editor and reorderable Home widgets are also deferred.

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

Current semantic version: **0.1.4-alpha**. See [CHANGELOG.md](CHANGELOG.md) and [RELEASE.md](RELEASE.md).

## License

MIT. See [LICENSE](LICENSE).
