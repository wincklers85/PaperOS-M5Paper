# PaperOS Architecture

## Design principles

1. **Original M5Paper first.** No S3-specific APIs.
2. **Offline first.** Core PDA/storage functions must not require Internet.
3. **E-paper aware.** Full refreshes are exceptional; low-change UI should use fast/partial refresh modes.
4. **Browser as the large control surface.** Heavy configuration belongs in the responsive Web UI rather than a tiny on-device keyboard.
5. **No fake hardware controls.** A module remains Coming Soon until a real service/API exists.
6. **Recoverable persistence.** Critical config writes use temporary/backup files.
7. **Services must fail independently.** Missing SD or network should not stop boot.

## Boot flow

```text
ESP32 reset/wake
  -> M5Unified init
  -> e-paper splash
  -> LittleFS config + recovery
  -> microSD mount + directory provisioning
  -> power service
  -> known Wi-Fi attempt
       -> connected: mDNS + NTP
       -> failed: PaperOS-Setup + captive DNS
  -> HTTP server/API
  -> first-boot instruction or Home UI
  -> cooperative main loop
```

Deep-sleep wake is a cold application start by ESP32 design. E-paper preserves the pre-sleep image.

## Modules

### Core

`ConfigManager` owns persistent global settings. `Logger` emits Serial logs and rotates `/PaperOS/Logs/system.log` on SD when it exceeds 256 KB.

### Drivers

`DisplayManager` initializes M5Unified, sets the e-paper mode and owns splash/full/fast refresh behavior. M5Unified owns the actual IT8951/GT911/BM8563 integration.

### Storage

`StorageManager` owns the shared SPI microSD mount and enforces a `/PaperOS` path sandbox. It exposes list/search/mkdir/remove/rename/copy primitives.

### Network

`WiFiManager` manages stored networks, reconnect, `PaperOS-Setup`, captive DNS and `paperos.local` mDNS.

### Services

`NotesService` persists one JSON file per note. `PowerManager` owns battery readings, inactivity behavior, sleep screen and deep sleep.

### UI

`UiManager` implements the physical M5Paper experience. The first milestone contains Home, launcher, Notes viewer, Settings and System Monitor. Unimplemented apps are explicitly labelled Coming Soon.

### Web

`WebServerService` serves static LittleFS assets and same-origin REST APIs. Authentication is a single in-RAM session token backed by a salted admin password hash. OTA uses the ESP32 Update API and dual OTA partitions.

## UI design system contract

Starting with 0.1.1-alpha, the physical M5Paper UI targets the native 540×960 portrait canvas and follows a shared design system rather than per-screen ad-hoc drawing.

The UI layer should converge on reusable primitives for:

- status bar and system indicators
- dashboard cards
- bottom navigation
- app tiles
- list rows
- buttons and confirmation modals
- consistent margins, spacing, radii and typography scales

Screen code should describe content and interaction while shared components own geometry and visual styling. E-paper updates should redraw the smallest practical region, reserving periodic full-quality refreshes for ghosting control. New visible controls must either work or be explicitly labelled Coming Soon.

## Concurrency model

0.1.1-alpha deliberately uses a cooperative Arduino loop rather than a large task graph. HTTP, UI, Wi-Fi DNS and power checks are short, bounded calls. Future MQTT/BLE/serial work can move long-lived I/O to FreeRTOS tasks/queues where it has a measurable benefit.

## Future service boundaries

Planned interfaces:

- `TaskService`
- `CalendarService`
- `BluetoothService`
- `MqttService`
- `DeviceRegistry`
- `SensorManager`
- `GpioService`
- `SerialService`
- `AutomationEngine`
- `NotificationService`
- `ReaderService`
- `WidgetManager`

Each should expose data models independent from both the e-paper UI and the Web UI so either frontend can use the same backend.


## Professional UI layer (0.1.1-alpha)

The on-device interface now has a dedicated reusable design layer in `src/ui/UiTheme.*`. It owns the 540×960 visual constants and shared components used by Home, Apps, Settings, System and Coming Soon screens:

- status-area geometry and e-paper-safe spacing
- cards, pills, app tiles and chevrons
- Wi-Fi, Bluetooth, microSD and battery status glyphs
- five-item bottom navigation
- consistent typography and black/white/gray treatment

`UiManager` remains responsible for navigation, real telemetry binding and touch routing. Unimplemented hardware modules are never populated with fake telemetry; they route to an explicit Coming Soon screen.

The browser console in `data/` uses the same product language but a responsive web layout. It consumes the existing authenticated REST API for live dashboard telemetry, files, notes, Wi-Fi, settings, device actions and OTA. No cloud service is required for local operation.
