# PaperOS Roadmap

PaperOS uses semantic versioning. Features move to a release only after the backend is real and the previous milestone still passes regression tests.

## 0.1.x Alpha — stable platform base

Implemented in 0.1.0-alpha:

- boot/splash, M5Paper display/touch/buttons
- RTC, battery, Wi-Fi/AP/mDNS
- browser first-boot setup
- authenticated Web UI + REST base
- microSD file manager
- Notes storage/editor/autosave
- persistent settings + recovery backup
- System Monitor + logs
- power/deep sleep
- remote screen actions
- firmware OTA

0.1.x stabilization targets:

- real hardware compile/flash regression matrix
- on-device first-boot controls beyond browser-assisted setup
- on-device Notes editor
- session/API token hardening
- filesystem OTA package support
- richer config backup/restore
- tighter e-paper dirty-region scheduler

## 0.2.x — organizer

- Tasks with LOW/NORMAL/HIGH/CRITICAL priority
- local Calendar: today/week/month/agenda
- reminders + Notification Center
- configurable/reorderable Home widgets
- lock screen + optional PIN
- Italian/English string catalog throughout device UI

## 0.3.x — connectivity / technical tools

- BLE Manager + BLE Console
- MQTT client/monitor with reconnect, QoS and retained publish
- safe GPIO Manager with reserved-pin protection
- UART Serial Terminal, ASCII/HEX, logs and presets
- RS232/RS485/Modbus RTU extension interface

## 0.4.x — automation / home / energy

- generic Smart Home device registry
- Solar dashboard
- configurable MQTT/HTTP data sources
- Automation Engine: IF / AND / THEN
- notifications from rules
- puffer/heating widgets

## 0.5.x — reader

- TXT and Markdown reader
- PDF capability evaluation and constrained renderer
- EPUB if memory/performance testing proves practical
- bookmarks, last page, crop, dithering and reading settings

Complex PDFs that exceed realistic ESP32 RAM/decoder limits will never be presented as universally supported. A documented preprocessing/server-assisted path will be used instead.

## 1.0.0

Criteria: hardware-tested original M5Paper support, crash/recovery soak tests, stable backup/restore, security review, OTA rollback validation, documented API and reproducible tagged build.
