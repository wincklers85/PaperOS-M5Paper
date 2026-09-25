# PaperOS

**PaperOS 0.3.1-alpha** is an offline-first, e-paper-native PDA firmware for the **original M5Stack M5Paper (ESP32-D0WDQ6-V3)**. It is intentionally not an Android clone. The architecture is designed around limited RAM, PSRAM, low power, e-paper refresh constraints, microSD storage and browser-based administration.

> Target: M5Stack M5Paper first generation only. M5Paper S3 is not the target of this repository.

## What is real in 0.3.1-alpha

- M5Unified/M5GFX initialization for original M5Paper
- boot splash and touch UI
- pixel-precise 540×960 Home, Apps, Settings, System Monitor, native Notes, native Files and Tools screens
- hardware buttons: Home / Apps / hold power for sleep
- RTC display and battery telemetry
- buffered one-pass 540×960 rendering: screens are composed off-screen, then committed with `epd_fastest`; only live regions use partial refresh
- clean anti-ghost transition when navigating to another page, with buffered one-pass redraw and fast regional updates inside the same app
- dedicated Solar and Termo UI shells marked Coming Soon, with no fake telemetry
- soft-lock standby keeps BLE/Wi-Fi/Phone Link alive; wheel-click or center button arms unlock, then a right swipe unlocks to Apps
- native Wi-Fi Analyzer with asynchronous scan, RSSI/channel/security details, touch network selection, on-device password entry, saved networks, reconnect and Setup AP (`PaperOS-Setup`)
- native BLE Inspector with scrollable scans, manufacturer/company identification, beacon classification, connection testing and read-only GATT service/characteristic inspection
- native Calculator utility
- native Clock desk display, Focus 25-minute timer and Fun random tools (dice / coin / 1–100)
- native Web Reader with DuckDuckGo non-JavaScript text search, HTTP/HTTPS reader mode, relative-link navigation and scrolling
- native RFC6238 TOTP calculator with volatile-only Base32 secret storage
- native **iPhone Link / Apple ANCS** accessory mode with secure BLE bonding, live iOS notifications and supported notification actions; the Phone dialer UI needs a compatible iPhone companion app to place calls or send SMS
- four persistent native UI themes: Soft, Classic, Technical and Minimal
- frosted/depth Quick Settings overlay designed for monochrome e-paper
- optional **PN532 NFC Lab** on Port C UART (G18/G19) with module diagnostics and ISO14443A UID scanning
- **Classic Desktop / Windows 3.11 mode** with startup splash, Program Manager, ROOT Terminal, BLE mouse/keyboard, Ski and Solitaire
- full microSD root browsing plus native File Manager Cut / Copy / Paste, long-press context actions, persistent Home file/folder shortcuts and a USB source status screen
- bottom-up Notification Center with ANCS calls/messages/mail, phone connection state and companion-supplied phone battery
- BLE mouse-wheel menu navigation with visible focus and wheel-click OK/Enter
- reusable on-screen touch keyboard with pressed-key feedback and SHOW/HIDE for passwords
- native Battery / Power Center with real rolling voltage graph, wake diagnostics, charging-likely estimation and clearly-labelled subsystem impact estimate
- Web Power Center plus extended battery REST telemetry
- WinLabs Solutions Labs area with beta tools and HID script library
- captive DNS redirect to `192.168.4.1`
- mDNS hostname `paperos.local`
- password-protected responsive Web UI
- first-boot browser-assisted wizard
- microSD layout and scrollable file manager: list, upload, download, create folder, rename, copy and delete; native text, JPEG/PNG/BMP and PDF-Lite readers
- SD Recovery combines FAT32 deleted-directory entries with an asynchronous, read-only, sector-by-sector carver for JPEG, PNG, BMP, PDF and printable text runs. It reports progress, can be stopped, previews supported images/text and exports through the authenticated browser. Raw carving loses names/folders, assumes contiguous file data and may return fragments or false positives; when FAT32 is recognized, candidate starts are checked against free clusters. SD writes are blocked during scanning. Optional same-card save still needs two confirmations, stages the full candidate and is capped at 1 MiB. NTFS/APFS metadata recovery is unsupported.
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
- **LAN Discovery** — scans the current IPv4 `/24` in four bounded ranges (`.1-.64`, `.65-.128`, `.129-.192`, `.193-.254`) for common TCP services on ports 22, 80, 443, 1883 and 8080. It is a lightweight Fing-style service discovery tool, not a guaranteed inventory of hosts that expose no tested service.
- **HTTP/API Tester** — GET, POST and PUT with response code/body.
- **MQTT Client** — connect, subscribe, publish and show the latest message on a topic. The alpha client targets plain local MQTT on port 1883; TLS/authentication are not yet implemented.
- **Wake-on-LAN** — sends a standard magic packet to the current subnet broadcast address.

BLE Inspector also recognises common iBeacon/Eddystone advertising markers and offers a **read-only GATT Reader** for connectable BLE devices. Only characteristics advertising the READ property are read; PaperOS does not write characteristics from this tool. AirTag owner identity and speaker control are not available to a generic BLE scanner; use Apple's Find My app with the Apple Account that owns or shares the AirTag.

## First web setup

During first boot, join the `PaperOS-Setup` Wi-Fi network and open `http://192.168.4.1`. The hostname `http://paperos.local` is available after PaperOS has joined the same local Wi-Fi network as the browser. If the emergency setup page says the Web UI files are missing, connect over USB and upload LittleFS with `pio run -e m5paper -t uploadfs`.

## Phone, USB and SD recovery limits

Phone Link can display iOS notifications provided by ANCS. Placing calls and composing SMS requires a separately installed iPhone companion that implements PaperOS's BLE command bridge; this repository does not include an iOS app. The File Manager transfer menu offers Wi-Fi download through the authenticated Web Console; the current BLE phone bridge does not implement binary file transfer. On-device USB shows host availability, but the original M5Paper has no onboard USB host controller; USB storage needs external host hardware. SD Recovery currently scans FAT32 deleted directory entries only, not every raw sector, and remains best-effort: it may miss files, and fragmented files may be incomplete. NTFS and APFS are not supported by the recovery scanner. Browser export is the safest destination. The optional same-card save stages files up to 1 MiB in PSRAM before writing to `/PaperOS/Recovery`, but its allocation can still overwrite other deleted files. Stop using the card after deletion because normal filesystem activity can overwrite recoverable sectors.

## GPIO Port map

Original M5Paper expansion signals used by GPIO Lab:

- Port A: GPIO25 / GPIO32
- Port B: GPIO26 / GPIO33
- Port C: GPIO18 / GPIO19

PaperOS initializes all six as INPUT. Signal I/O is 3.3 V logic; the Grove/HY2.0 red supply wire is 5 V.

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
- **iPhone Link** receives current iOS notifications natively through Apple ANCS after BLE bonding. Full Contacts, message history and call history are not ANCS data sources. Camera, sending messages and initiating calls remain companion-controlled functions. See [PHONE_LINK.md](docs/PHONE_LINK.md).
- **PDF Reader Lite** extracts readable text from compatible/uncompressed PDF content; it is not a full graphical PDF engine.
- **Web Reader** does not execute JavaScript, video or complex CSS; HTTPS currently uses lightweight transport without CA validation.
- **SD formatting** supports one whole-card FAT/exFAT PaperOS volume. Multi-partition mounting/editing is intentionally not exposed yet.
- **Battery current in mA** is unavailable without an external current monitor.
- **Wi-Fi Audit** is diagnostic only: no deauth, credential capture, password cracking or access bypass.
- Solar/Termo controls, on-device text editing, reorderable Home widgets, NTFS/APFS metadata recovery, fragmented-file reconstruction, on-device text editing and a packaged phone companion app remain deferred. The raw carver is implemented but still needs disposable-card hardware validation. Home currently supports two persistent file/folder shortcuts.

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

Native File Manager can browse the full microSD root. Web/API file operations also use validated absolute SD paths.

## Release

Current semantic version: **0.3.1-alpha**. See [CHANGELOG.md](CHANGELOG.md) and [RELEASE.md](RELEASE.md).

## License

MIT. See [LICENSE](LICENSE).
