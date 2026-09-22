# 0.1.0-alpha test matrix

## Build
- `pio run -e m5paper`
- `pio run -e m5paper -t buildfs`

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

## Storage
- SD absent: boot still succeeds
- SD present: all `/PaperOS/*` directories created
- browser upload/list/download/mkdir/rename/copy/delete
- note create/edit/autosave/delete survives reboot

## Power / OTA
- deep-sleep screen remains visible
- touch/power wake returns through cold boot
- timer wake tested with USB disconnected if RTC/timer behavior requires it
- OTA rejects invalid/incomplete image and reboots only after successful `Update.end(true)`

## Soak
- 24 h connected run
- repeated Wi-Fi loss/recovery
- 100 partial top-bar refreshes followed by quality refresh
