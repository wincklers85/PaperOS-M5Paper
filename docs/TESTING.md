# 0.3.1-alpha test matrix

## Build
- `pio run -e m5paper`
- `pio run -e m5paper -t buildfs`
- `node --check data/app.js`
- `python scripts/preflight.py`

## First flash
- firmware upload
- LittleFS upload
- splash appears in portrait orientation
- touch coordinates match visual cards
- PaperOS-Setup AP appears with no saved Wi-Fi
- captive redirect / 192.168.4.1 loads
- setup creates password and network config
- reboot joins LAN
- `paperos.local` resolves on same LAN
- repeat setup access from an iPhone on the setup AP and on the configured LAN
- verify status bar, touch, wheel/click and hardware-button navigation after reboot

## Storage
- SD absent: boot still succeeds
- SD present: boot does not create folders, append logs or export configuration automatically
- explicit Notes/configuration saves create only the required folders
- browser upload/list/download/mkdir/rename/copy/delete
- note create/edit/autosave/delete survives reboot
- FAT32 recovery scan leaves source sectors unchanged until an explicit save is selected
- preview supported JPEG/PNG/BMP/text candidates and export to the browser device
- same-card restore: use only a disposable test card; verify two confirmations, 1 MiB cap and `/PaperOS/Recovery` destination
- verify unsupported formats produce a clear status (NTFS, APFS and exFAT recovery)

## Power / OTA
- deep-sleep screen remains visible
- touch/power wake returns through cold boot
- timer wake tested with USB disconnected if RTC/timer behavior requires it
- OTA rejects invalid/incomplete image and reboots only after successful `Update.end(true)`

## Soak
- 24 h connected run
- repeated Wi-Fi loss/recovery
- 100 partial top-bar refreshes followed by quality refresh
- record board revision, firmware SHA, free heap/PSRAM, boot time, battery behavior and test results
