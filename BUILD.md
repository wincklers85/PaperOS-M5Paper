# BUILD — PaperOS 0.1.0-alpha

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

`.github/workflows/build.yml` runs JavaScript syntax checking, PaperOS preflight checks and `pio run -e m5paper`. Pushing this repository to GitHub therefore provides a clean-toolchain compilation check independent of a developer workstation.

## Current validation note

This source tree was generated in an environment where the PlatformIO executable/toolchain was not preinstalled and outbound package installation was unavailable. Static repository checks and JavaScript syntax checks can run locally here; the definitive ESP32 toolchain compilation is therefore delegated to PlatformIO/CI using the commands above. Do not treat a release as hardware-validated until that compile and an actual M5Paper boot test pass.
