# BUILD — PaperOS 0.3.2-alpha

## Requirements

- original M5Stack M5Paper (ESP32, not S3)
- data-capable USB cable
- VS Code + PlatformIO, or PlatformIO Core
- microSD recommended for Notes, Files and Logs

## Build profile

`platformio.ini` intentionally uses:

- platform: `espressif32@6.12.0`
- framework: Arduino
- base board: `esp32dev`
- target guard: `PAPEROS_TARGET_M5PAPER_V1=1`
- flash: 16 MB
- PSRAM enabled
- filesystem: LittleFS
- M5Unified 0.2.23
- ArduinoJson 6.21.5

The custom partition table provides two 6 MB OTA application slots and the remaining flash to LittleFS.

## Commands

```bash
# Compile firmware
pio run -e m5paper

# Flash firmware over USB
pio run -e m5paper -t upload

# Flash Web UI / LittleFS
pio run -e m5paper -t uploadfs

# Serial monitor
pio device monitor -b 115200

# Debug build
pio run -e m5paper-debug
```

If auto-detection chooses the wrong serial port:

```bash
pio run -e m5paper -t upload --upload-port /dev/ttyUSB0
```

On macOS the port is usually `/dev/cu.SLAB_USBtoUART` or a similarly named CP210x device. On Windows it is normally a `COM` port.

## First flash

For a fresh device, flash **both** the application and LittleFS. The firmware binary and Web UI filesystem are intentionally separated in this alpha. Firmware OTA updates do not replace LittleFS yet.

## Build artifacts

After a successful build:

```text
.pio/build/m5paper/firmware.bin
.pio/build/m5paper/littlefs.bin   # after buildfs/uploadfs
```

The Web UI OTA page accepts `firmware.bin` only.

## CI

`.github/workflows/build.yml` runs JavaScript syntax checking, PaperOS preflight checks, `pio run -e m5paper` and `pio run -e m5paper -t buildfs` from a clean checkout. A green workflow confirms compilation and filesystem packaging, not hardware behavior.

## Current validation note

In the current development environment, static repository checks and JavaScript syntax checks can run locally, but PlatformIO is not installed. Use the CI result for firmware compilation. Do not mark a release hardware-validated until an original M5Paper has completed the installation and test matrix in `docs/TESTING.md`.
