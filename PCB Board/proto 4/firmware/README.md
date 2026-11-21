# GEOpath Finder offline map viewer

Firmware for the ESP32-S3 Proto 4 board. It reads pre-rendered offline map tiles from microSD, keeps the current GPS position centered, and shows compass heading. The map remains north-up; the compass arrow indicates magnetic heading.

## Build and flash

Open this folder with PlatformIO and build/upload the `esp32-s3-devkitc-1` environment. Connect the board by USB and use PlatformIO's Serial Monitor at 115200 baud for startup status. Confirm the flash/upload settings for the exact ESP32-S3-WROOM module fitted to the PCB.

## Prepare the SD map

The firmware reads **256 × 256 PNG XYZ/Web Mercator map tiles** converted to raw RGB565 files. Its fixed zoom level is `MAP_ZOOM` in `src/main.cpp` (default 16). The card must contain tile paths for that zoom, for example:

```text
SD card/
└── maps/
    └── 16/
        └── 10482/
            └── 25333.rgb565
```

Use a map export/cache tool that can save XYZ raster tiles for the area and zoom you need. Keep the source tiles in a standard `z/x/y.png` directory tree, then convert them on your computer:

```bash
python -m pip install Pillow
python tools/tiles_to_rgb565.py ./xyz_png_tiles ./converted_maps
```

Copy the converted `16` folder into `/maps` on the SD card. Each RGB565 file is 131,072 bytes. At zoom 16, the device needs the tiles covering your location; include neighboring tiles so the display can pan smoothly as GPS moves. Map imagery can be large: select only the geographic area and zoom levels you need. This firmware currently uses one configured zoom level and does not download maps.

Use map data and tile exports whose license and usage terms permit offline storage. The converter only changes image encoding; it does not supply map data.

## Board wiring from the schematic

| Signal | ESP32-S3 GPIO |
|---|---:|
| I²C SDA / SCL | 8 / 9 |
| SPI MOSI / SCK / MISO | 11 / 12 / 13 |
| microSD chip select | 18 |
| Display CS / D-C / reset | 40 / 39 / 38 |
| ICM-20948 | I²C address `0x69` |
| NEO-M8N | I²C/DDC address `0x42` |

GPS UART pins are unconnected in the schematic, so the firmware uses the NEO-M8N's DDC/I²C pins on the shared I²C bus. The display and SD card share SPI with separate chip selects.

## Display and compass notes

The screen is configured in portrait (`320 × 480`, `setRotation(0)`). This assumes the ER-TFT040-5 is configured for 4-wire SPI and the ILI9488 driver is compatible with the panel's FPC strap option. Confirm the exact module pinout/strap and logic voltage before powering the panel. The schematic routes a resistive touch controller, but this initial map viewer does not use touch input; zoom is set in firmware. Compass heading is an uncalibrated, level-device magnetic heading. Calibrate hard/soft iron offsets after assembly for dependable direction readings.
