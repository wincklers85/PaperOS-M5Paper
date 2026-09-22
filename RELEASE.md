# Release 0.1.1-alpha

## Purpose

Incremental alpha milestone for the original M5Stack M5Paper, establishing the professional 540×960 GUI design-system direction while preserving the installable 0.1.x architecture.

## Release checklist

- [x] repository layout created
- [x] semantic version embedded
- [x] dual-OTA partition layout
- [x] Web UI filesystem payload
- [x] static preflight checks
- [x] JavaScript syntax check
- [ ] clean PlatformIO compile
- [ ] flash to original M5Paper
- [ ] verify touch coordinate orientation
- [ ] verify microSD read/write with display active
- [ ] verify Setup AP on iPhone/Android
- [ ] verify `paperos.local` on target LAN
- [ ] verify deep-sleep touch + timer wake
- [ ] verify OTA and rollback path
- [ ] 24-hour stability/heap test

Do not remove the `alpha` suffix until the unchecked hardware/toolchain tests pass.
\n## Professional UI milestone\n\n0.1.1-alpha now includes the production-oriented 540×960 PaperOS device design system and the responsive authenticated browser console. The browser controls are wired to the existing REST endpoints for live status, files, Notes, Wi-Fi, settings, remote device actions and OTA.\n