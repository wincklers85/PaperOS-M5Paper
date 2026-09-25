# PaperOS Roadmap

**Target:** first-generation M5Stack M5Paper with classic ESP32. M5Paper S3 is not supported by this roadmap.

**Current baseline:** 0.3.1-alpha on `main` (2026-09-25). The repository already contains the core device UI, Web UI, file manager, Notes, Wi-Fi tools, BLE/Phone Link, NFC Lab, GPIO/serial/network tools, Classic Desktop, and constrained readers. This roadmap focuses on finishing, validating, and documenting them rather than treating already-shipped work as future work.

A feature is complete only when its backend and UI work together, CI passes, and its acceptance checks are recorded. Hardware-dependent features remain provisional until tested on an original M5Paper.

## 0.3.2-alpha — installation and stabilization

**Goal:** make the current feature set straightforward to install and safe to evaluate on real hardware.

- [ ] Run a clean firmware and LittleFS build from documented instructions.
- [ ] Test first flash and first-boot setup on the original M5Paper, including setup AP from an iPhone.
- [ ] Verify portrait orientation, touch mapping, physical buttons, Wi-Fi reconnect, RTC, battery display and microSD read/write.
- [ ] Fix issues found during the user's installation and record the tested board/revision.
- [ ] Confirm the boot/recovery path when the microSD is absent or unreadable.
- [ ] Update version-stale BUILD, RELEASE, API, TESTING, SECURITY and hardware documentation to describe 0.3.1+ accurately.
- [ ] Keep all unsupported or partial functions clearly labelled in the UI and README.

**Exit gate:** clean CI build, successful install and boot on the original M5Paper, first-boot setup completed, and core storage/network checks recorded.

## 0.4.x — PDA essentials

**Goal:** complete the everyday organizer functions.

- [ ] Tasks with priorities, completion state, due dates and persistence.
- [ ] Local calendar with agenda, day, week and month views.
- [ ] Reminders and a notification path shared by local reminders and existing Phone Link notifications.
- [ ] Configurable and reorderable Home widgets.
- [ ] Optional lock-screen PIN with clear recovery behavior.
- [ ] Complete Italian and English strings across device UI, Web UI and user-facing messages.
- [ ] Finish a practical on-device Notes editor; retain browser editing and autosave.

**Exit gate:** tasks, events, reminders, language selection and settings persist across reboot and work offline.

## 0.5.x — reliability, storage and update safety

**Goal:** make user data and updates recoverable.

- [ ] Define and test a backup/restore format for settings, Wi-Fi configuration, Notes and user-selected SD content.
- [ ] Make restore validate its input and recover safely from interrupted writes.
- [ ] Exercise dual-slot firmware OTA, invalid-image rejection and rollback after a failed boot.
- [ ] Decide and implement a safe LittleFS/Web UI update path, or document a reliable USB recovery procedure.
- [ ] Add regression checks for file operations, Notes, setup, authentication and OTA.
- [ ] Review session handling, password verification, cookie settings, destructive API operations and local-network exposure; document remaining limits.
- [ ] Ensure secrets are not exposed in logs or API responses, and explain how stored Wi-Fi credentials are protected.
- [ ] Test storage-full, missing-SD, Wi-Fi-loss and interrupted-update recovery.

**Exit gate:** backup/restore and update recovery pass repeatable tests, with no loss of existing user data in the supported recovery scenarios.

## 0.6.x — real home and energy integrations

**Goal:** turn Solar and Termo from planned screens into useful features connected to real data.

- [ ] Add a device/sensor registry with explicit connection state, units, freshness and errors.
- [ ] Support documented MQTT and HTTP data sources; show configuration and connection errors clearly.
- [ ] Implement Solar and Termo dashboards only for configured, real data sources. Never show fabricated telemetry.
- [ ] Add puffer/heating widgets after the relevant sensors are configured.
- [ ] Add an automation engine for bounded IF/AND/THEN rules and notifications.
- [ ] Require explicit configuration and safe defaults for any rule that can control GPIO or heating hardware.
- [ ] Document supported wiring, pin reservations, voltage limits and tested modules.

**Exit gate:** dashboards show source, last update and unavailable states correctly; rules can be tested without actuating hardware before being enabled.

## 0.7.x — reader and daily-use polish

**Goal:** make document reading dependable within original ESP32 limits.

- [ ] Improve TXT and Markdown reading, navigation, bookmarks and last-read position.
- [ ] Improve PDF-Lite compatibility reporting and error handling; document unsupported PDF features.
- [ ] Add reading settings such as crop and dithering where memory and refresh performance allow.
- [ ] Evaluate EPUB with measured RAM, speed and stability. Ship it only if it works reliably on the target.
- [ ] Keep complex PDF/EPUB support explicitly constrained; offer a documented preprocessing path if needed.
- [ ] Run a usability pass over navigation, empty states, confirmations and e-paper refresh behavior.

**Exit gate:** supported files open reliably from microSD, navigation state persists where promised, and unsupported files fail with a useful explanation.

## 0.8.x — release candidate

**Goal:** freeze scope and validate the complete 1.0 candidate.

- [ ] Complete a documented regression matrix on original M5Paper hardware.
- [ ] Run at least a 24-hour soak test and repeated reboot, Wi-Fi loss/recovery, SD removal/reinsert and low-storage tests.
- [ ] Record heap/PSRAM behavior, boot time, battery behavior and display-refresh checks.
- [ ] Review permissions and security notes against the shipped code and configuration.
- [ ] Verify clean build and firmware/LittleFS installation from a fresh checkout using pinned dependencies.
- [ ] Verify backup/restore and OTA rollback using release-candidate artifacts.
- [ ] Update README, BUILD, API, SECURITY, HARDWARE, TESTING, CHANGELOG and RELEASE documents.
- [ ] Resolve all release-blocking defects; publish a candidate build and collect device-test feedback.

**Exit gate:** no known release-blocking defects; all mandatory 1.0 checks below have evidence.

## 1.0.0 — release criteria

PaperOS 1.0.0 can ship when all of these are true:

- [ ] Builds reproducibly from a clean checkout and CI passes for firmware and LittleFS.
- [ ] Installs and boots on the first-generation M5Stack M5Paper; supported hardware behavior is documented.
- [ ] Core UI, settings, Notes, file manager, Wi-Fi setup and offline operation pass the hardware regression matrix.
- [ ] Data backup/restore and interrupted-update recovery have been tested.
- [ ] Firmware OTA failure and rollback behavior have been validated on hardware.
- [ ] A stability soak and recovery tests pass with results recorded.
- [ ] Security review is complete and known limitations are stated plainly.
- [ ] API, installation and troubleshooting documentation matches the released firmware.
- [ ] Release version, changelog, tagged source and build artifacts correspond to the same tested commit.

## Scope and hardware limits

- USB-C on the original M5Paper is a USB-to-serial bridge, not native USB HID. The BadUSB Lab is a script library; direct HID execution through the built-in port is not promised.
- Battery current and exact charge state are unavailable from the original board without an external sensor.
- Web Reader does not run JavaScript or complex browser content. HTTPS certificate-validation limits must be disclosed until resolved.
- MQTT security capabilities must be stated accurately; do not present an unencrypted local connection as secure.
- EPUB and complex PDF support depend on measured memory/performance and are not unconditional 1.0 promises.
- No Solar, Termo, GPIO or automation screen may imply live measurements or control unless it is connected to a real configured backend.
