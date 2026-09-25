# Release 0.3.2-alpha — installation and stabilization

## Purpose

Stabilize the current PaperOS feature set on the original M5Stack M5Paper (classic ESP32). This release must prove clean installation and core device behavior before development moves on to 0.4.x PDA features.

## Release checklist

- [x] Static repository preflight passes locally.
- [x] Web UI JavaScript syntax check passes locally.
- [ ] Clean firmware and LittleFS CI build passes.
- [ ] Flash firmware and LittleFS to original M5Paper (not M5Paper S3).
- [ ] Verify 540×960 portrait layout, touch coordinates, wheel/click and hardware buttons.
- [ ] Verify setup AP from iPhone, setup completion and reconnect on target LAN.
- [ ] Verify `paperos.local` on target LAN.
- [ ] Verify RTC, battery display, SD behavior and boot with missing/unreadable SD.
- [ ] Verify OTA success, invalid-image rejection and recovery/rollback behavior.
- [ ] Complete the storage, power and soak tests in `docs/TESTING.md`.
- [ ] Record the tested board revision, firmware commit, results and defects.

Do not tag or call 0.3.2-alpha hardware-validated until every required build and device test above has recorded evidence.
