# GEOpath Finder firmware

Arduino/PlatformIO starter firmware for the ESP32-S3-WROOM-1 on Proto 4. It reads a route from microSD, obtains position from the NEO-M8N over I2C/DDC, reads the ICM-20948 magnetometer, and displays distance/bearing prompts on the ER-TFT040-5 with XPT2046 resistive touch.

## Build

Open this folder in PlatformIO and build/upload `esp32-s3-devkitc-1`. The board definition is a convenient ESP32-S3 target; confirm flash size and USB upload settings against the module variant before deployment.

Copy `data/route.csv` to the SD card root. Replace the sample coordinates. Format:

```csv
latitude,longitude,label
37.421998,-122.084000,Start
37.422100,-122.083800,Turn
```

Lines beginning with `#` are ignored. Up to 300 waypoint rows are loaded.

## PCB-derived GPIO mapping

| Signal | ESP32-S3 GPIO | Schematic net |
|---|---:|---|
| I2C SDA | 8 | SDA PIN ESP |
| I2C SCL | 9 | SCL PIN ESP |
| SPI MOSI | 11 | MOSI IN ESP |
| SPI SCK | 12 | SCK/CLK ESP |
| SPI MISO | 13 | MISO IN ESP |
| microSD CS | 18 | SD socket DAT3 / GPIO net |
| Display CS | 40 | CSX Display |
| Display D/C | 39 | DCX Display |
| Display reset | 38 | RESET DISPLAY |
| XPT2046 CS | 41 | CS XPT2046 |
| XPT2046 IRQ | 42 | PENIRQ XPT2046 |

ICM-20948 address is strapped to `0x69`; NEO-M8N DDC address is `0x42`. The NEO UART pins are unconnected in the schematic, so the code uses its I2C/DDC interface. Verify the GNSS module's DDC mode/address and I2C pull-ups on the assembled board.

## Display configuration and calibration

ER-TFT040-5 uses an ILI9488. This firmware assumes the panel's IM straps select 4-wire SPI and the exact panel option is compatible with the Adafruit ILI9488 SPI driver. Confirm the display datasheet/FPC strap configuration before applying power; the schematic connector does not show the IM strap pins. The schematic wires the four resistive touch electrodes to an XPT2046 controller. Touch coordinates need calibration for the installed glass orientation. Magnetometer heading is raw and should be hard/soft-iron calibrated after assembly; tilt compensation is not yet applied.

The screen shows the next waypoint, straight-line distance, bearing, and a simple left/right cue. A waypoint is considered reached within 8 m. Tap the lower-left touchscreen region to skip a point after touch calibration.
