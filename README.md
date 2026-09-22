# PaperOS

**PaperOS 0.1.0-alpha** is an offline-first, e-paper-native PDA firmware for the **original M5Stack M5Paper (ESP32-D0WDQ6-V3)**. It is intentionally not an Android clone. The architecture is designed around limited RAM, PSRAM, low power, e-paper refresh constraints, microSD storage and browser-based administration.

> Target: M5Stack M5Paper first generation only. M5Paper S3 is not the target of this repository.

## What is real in 0.1.0-alpha

- M5Unified/M5GFX initialization for original M5Paper
- boot splash and touch UI
- Home, Apps, Settings and System Monitor screens
- hardware buttons: Home / Apps / hold power for sleep
- RTC display and battery telemetry
- Wi-Fi saved networks, reconnect and Setup AP (`PaperOS-Setup`)
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

## Explicitly not implemented yet

The Web UI displays these modules as **Coming Soon** rather than simulating them: Tasks, Calendar, BLE Console, MQTT, Smart Home, Solar, GPIO Manager, Serial Terminal, Automations, Notifications, PDF Reader and Ebook Reader. The on-device note editor and reorderable Home widgets are also deferred.

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

Current semantic version: **0.1.0-alpha**. See [CHANGELOG.md](CHANGELOG.md) and [RELEASE.md](RELEASE.md).

## License

MIT. See [LICENSE](LICENSE).
