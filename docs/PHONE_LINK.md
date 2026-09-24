# PaperOS Phone Link companion protocol

PaperOS Phone Link is a bridge protocol for the original M5Paper. A phone companion is required for phone-protected functions such as system notifications, SMS/messages, placing calls and controlling the phone camera.

## BLE GATT

Device advertising name: `PaperOS Phone Link`

Service UUID:

`6f0c1000-4e55-4c42-8a8d-50415045524f`

Notification input characteristic (companion -> PaperOS, WRITE / WRITE_NR):

`6f0c1001-4e55-4c42-8a8d-50415045524f`

Write UTF-8 JSON:

```json
{"app":"Messages","title":"Alice","body":"Hello"}
```

Command output characteristic (PaperOS -> companion, READ / NOTIFY):

`6f0c1002-4e55-4c42-8a8d-50415045524f`

The companion should subscribe to notifications on the command characteristic. PaperOS also leaves the latest command as the readable value.

## Command strings

The alpha protocol uses small UTF-8 commands so it is easy to implement in Android, iOS, Tasker or other companion software.

- `camera:shutter`
- `media:playpause`
- `phone:ring`
- `call:+391234567890`
- `message:+391234567890|Hello from PaperOS`

The companion decides whether it has the OS permissions to execute a command. PaperOS does not bypass Android/iOS permission controls.

## REST companion bridge

An authenticated companion on the same Wi-Fi network can also use:

- `GET /api/phone/status`
- `GET /api/phone/notifications`
- `POST /api/phone/notify`
- `POST /api/phone/command`
- `POST /api/phone/start`

Example notification body:

```json
{"app":"WhatsApp","title":"Stephan","body":"Test notification"}
```

Example command body:

```json
{"command":"camera:shutter"}
```

## Platform notes

Android can expose notifications through a companion app with Notification Listener permission. SMS/call actions require the corresponding Android permissions/default-role rules.

iPhone is more restricted. A future companion can use Apple-supported mechanisms such as notification accessories/ANCS where available, but PaperOS must not assume a generic BLE peripheral can directly read iOS messages or place calls without a companion and user permission.
