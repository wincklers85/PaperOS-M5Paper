# PaperOS iPhone Link / companion protocol

PaperOS 0.2.0-alpha supports two complementary phone paths on the original M5Paper.

## 1. Native iPhone notifications with Apple ANCS

PaperOS acts as a Bluetooth Low Energy accessory and advertises the Apple Notification Center Service (ANCS) solicitation UUID.

ANCS service:

`7905F431-B5CE-4E99-A40F-4B1E122D00D0`

PaperOS subscribes to:

- Notification Source: `9FBF120D-6301-42D9-8C58-25E699A21DBD`
- Data Source: `22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB`
- Control Point: `69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9`

After secure BLE bonding, PaperOS can receive current iOS notification events and request fields such as app identifier, title and message text. Incoming-call and missed-call notifications are distinguishable through ANCS categories. If iOS advertises positive or negative actions for a notification, PaperOS exposes those actions without assuming what the labels mean.

### First pairing on iPhone

1. Open **Phone Link** on PaperOS and tap **PAIR**.
2. On the iPhone, open **nRF Connect for Mobile**.
3. Scan for **PaperOS** and choose **Connect**.
4. Accept the iOS Bluetooth pairing request.
5. Allow notification access for the accessory if iOS prompts for it.
6. Return to PaperOS. The status should become **iPhone ANCS connected**.

A DIY BLE accessory may not always appear in the normal Settings > Bluetooth device list before a connection is initiated. nRF Connect is therefore the recommended first-pair tool and diagnostic fallback.

### What ANCS does not provide

ANCS is a notification service, not a database synchronization protocol. It does not provide:

- the complete iPhone Contacts database;
- historical Messages/SMS/iMessage conversations;
- complete call history;
- arbitrary access to other apps' private data.

Information contained in a current notification, such as a caller/contact name or message preview, can appear on PaperOS when iOS exposes it in that notification.

## 2. Optional PaperOS companion bridge

The custom bridge remains available for actions that ANCS does not provide, including workflows such as camera shutter, media control, initiating calls or sending messages when a companion app/automation has the corresponding phone permissions.

Custom service UUID:

`6f0c1000-4e55-4c42-8a8d-50415045524f`

Notification input characteristic (companion -> PaperOS):

`6f0c1001-4e55-4c42-8a8d-50415045524f`

Command output characteristic (PaperOS -> companion):

`6f0c1002-4e55-4c42-8a8d-50415045524f`

Example commands:

- `camera:shutter`
- `media:playpause`
- `phone:ring`
- `call:+391234567890`
- `message:+391234567890|Hello from PaperOS`

The phone companion decides whether the operating system permits each command. PaperOS does not bypass iOS or Android permission controls.

## REST companion bridge

An authenticated companion on the same Wi-Fi network can use:

- `GET /api/phone/status`
- `GET /api/phone/notifications`
- `POST /api/phone/notify`
- `POST /api/phone/command`
- `POST /api/phone/start`

Example forwarded notification:

```json
{"app":"WhatsApp","title":"Alice","body":"Hello"}
```

Example command:

```json
{"command":"camera:shutter"}
```
