#pragma once
#include <Arduino.h>

// Derived from GEOpath Finder PCB Proto 4 schematic netlist.
namespace Pins {
constexpr int I2C_SDA = 8;       // ESP32-S3 IO8, shared ICM-20948 + NEO-M8N DDC
constexpr int I2C_SCL = 9;       // ESP32-S3 IO9
constexpr int SPI_MOSI = 11;     // ESP32-S3 IO11
constexpr int SPI_SCK = 12;      // ESP32-S3 IO12
constexpr int SPI_MISO = 13;     // ESP32-S3 IO13
constexpr int SD_CS = 18;        // microSD DAT3/CS
constexpr int DISPLAY_CS = 40;   // FPC J11 pin 9
constexpr int DISPLAY_DC = 39;   // FPC J11 pin 10
constexpr int DISPLAY_RST = 38;  // FPC J11 pin 15
constexpr int TOUCH_CS = 41;     // XPT2046 CS
constexpr int TOUCH_IRQ = 42;    // XPT2046 PENIRQ
constexpr int GPS_ADDRESS = 0x42;
constexpr int IMU_ADDRESS = 0x69;
}
