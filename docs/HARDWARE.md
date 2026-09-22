# Hardware — original M5Paper only

PaperOS 0.1.x targets the first-generation M5Stack M5Paper with classic ESP32, 16 MB flash, 8 MB PSRAM, IT8951 e-paper controller, GT911 touch controller, RTC and microSD.

## Shared SPI bus

The display and microSD share the classic ESP32 SPI signals:

| Signal | GPIO |
|---|---:|
| SCLK | 14 |
| MOSI | 12 |
| MISO | 13 |
| E-paper CS | 15 |
| E-paper reset | 23 |
| E-paper busy | 27 |
| microSD CS | 4 |

Display/touch/power/RTC are initialized through M5Unified/M5GFX. PaperOS must not use M5Paper S3 pin maps or ESP32-S3 APIs.

## Safety rule for future GPIO app

Pins already used by display, touch, RTC, SD, USB/UART and power circuitry must be marked reserved and must not be switchable from the GPIO Manager.
