# PaperOS Classic Desktop 0.3.0-alpha

Classic Desktop is a native PaperOS application inspired by the Windows 3.11 interaction model. It does not emulate x86 Windows; the windows and programs call PaperOS services directly, which is considerably lighter on the original ESP32 M5Paper.

## Start

Open **Apps > Classic Desktop / Windows 3.11 Mode**.

PaperOS displays the 3.11-style splash and opens Program Manager.

## Program Manager

Available programs:

- ROOT Terminal
- File Manager
- Control Panel
- Bluetooth Input
- Ski
- Solitaire
- Network
- NFC / GPIO
- Exit to PaperOS

## Bluetooth mouse and keyboard

Classic Desktop currently supports **BLE HID** peripherals using standard HID-over-GATT service UUID `0x1812`.

Open **Bluetooth Input**, put the mouse or keyboard into pairing mode and press **SCAN**. Select **LINK** next to the device.

PaperOS subscribes to Boot Keyboard / Boot Mouse when available and also listens to HID Input Report characteristics. BLE mouse movement and primary click control the Classic pointer. BLE keyboard input types directly into the ROOT Terminal.

Bluetooth Classic HID is not enabled in this release.

## ROOT Terminal

The terminal has direct PaperOS privileges. It is not a fake DOS shell.

Common commands:

```
help
ver
sysinfo
heap
battery
time
sd
pwd
cd C:
cd D:
dir
ls
type <file>
cat <file>
mkdir <path>
del <path>
copy <source> <destination>
move <source> <destination>
wifi status
wifi on
wifi off
wifi scan
hid status
hid scan
hid connect <index>
nfc scan
gpio list
gpio read <pin>
gpio mode <pin> in|out|pullup
gpio write <pin> 0|1
gpio adc <pin>
unsafe on
unsafe off
reboot
sleep
```

Drive aliases:

- `C:` => `/PaperOS`
- `D:` => microSD root `/`

By default raw GPIO control is limited to expansion pins Port A/B/C: 25, 32, 26, 33, 18 and 19. `unsafe on` allows GPIO 0..39. This can interfere with the display, SD card or other internal hardware.

## File Manager

The PaperOS File Manager now exposes two roots:

- **PaperOS** => `/PaperOS`
- **SD Card /** => the complete mounted microSD

Use **Select**, tap a file/folder, then **Copy** or **Cut**. Navigate to the destination and press **Paste**. Folders are copied recursively and existing destination names are not silently overwritten.

## Ski

Use left/right touch areas, BLE mouse clicks or keyboard Left/Right and A/D. Obstacles move automatically; score increases as they pass.

## Solitaire

Klondike layout includes stock, waste, seven tableau columns and four foundations. Tap a source card then a destination. Tableau moves enforce descending alternating colors; foundations enforce ascending suit order. Press `N` on a BLE keyboard to deal a new game.
