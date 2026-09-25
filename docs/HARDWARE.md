# Hardware — original M5Paper only

PaperOS targets the first-generation M5Stack M5Paper with classic ESP32, 16 MB flash, 8 MB PSRAM, IT8951 e-paper controller, GT911 touch controller, RTC and microSD.

## PSRAM reporting

The original M5Paper has 8 MB of physical PSRAM. The classic ESP32 maps at most 4 MB into its normal address space, which is what ordinary pointers and the current Arduino PSRAM allocator use. The remaining memory requires the ESP-IDF Himem paging API; PaperOS does not currently use Himem. The device UI and Web Console therefore show both the directly usable amount reported by `ESP.getPsramSize()` and the board's installed 8 MB capacity. This is expected and does not indicate missing memory.

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
