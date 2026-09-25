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
- tap SD status in Settings > Storage to unmount; remove/reinsert card; tap again to mount; verify Files and Notes recover without reboot
- open a JPEG/PNG/BMP from SD repeatedly; verify the UI does not freeze and RAM returns after closing previews
- Settings > Reset: confirm settings-only reset preserves SD contents; confirm content reset removes only `/PaperOS` and resets preferences
- note create/edit/autosave/delete survives reboot
- run a complete sector scan on a disposable FAT32 card; verify progress, stop/cancel, JPEG/PNG/BMP/PDF/text signature handling and unchanged source sectors
- verify PaperOS file, upload, Notes and format writes are rejected while sector scanning is active
- preview supported JPEG/PNG/BMP/text candidates and export both directory-entry and raw-carved candidates to the browser device
- same-card restore: use only a disposable test card; verify two confirmations, 1 MiB cap and `/PaperOS/Recovery` destination
- verify unsupported/unmounted formats report limits clearly; test NTFS/APFS metadata recovery as unsupported and raw scans only when the SD driver exposes readable sectors

## Power / OTA
- Quick Settings > Power Off shows the power-off screen and shuts the original M5Paper down when running from battery; verify USB-powered behavior separately
- deep-sleep screen remains visible
- touch/power wake returns through cold boot
- timer wake tested with USB disconnected if RTC/timer behavior requires it
- OTA rejects invalid/incomplete image and reboots only after successful `Update.end(true)`

## Soak
- verify splash progress advances with initialized services and ends before Home is drawn
- pair an iPhone with nRF Connect and record the Phone Link status for success/failure; normal iOS Bluetooth Settings alone is not a supported first-pair path
- 24 h connected run
- repeated Wi-Fi loss/recovery
- 100 partial top-bar refreshes followed by quality refresh
- record board revision, firmware SHA, free heap/PSRAM, boot time, battery behavior and test results
