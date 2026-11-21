#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <ILI9488.h>
#include <Adafruit_ICM20948.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <math.h>
#include "board_pins.h"

// Offline raster map viewer. SD map tiles are 256x256 RGB565, little-endian,
// arranged as /maps/<zoom>/<x>/<y>.rgb565 (XYZ/Web Mercator tile coordinates).
ILI9488 tft(Pins::DISPLAY_CS, Pins::DISPLAY_DC, Pins::DISPLAY_RST);
Adafruit_ICM20948 imu;
SFE_UBLOX_GNSS gps;

static constexpr int SCREEN_W = 320, SCREEN_H = 480, TILE = 256;
static constexpr uint8_t MAP_ZOOM = 16;  // Change to the zoom level copied to SD.
static constexpr uint32_t TILE_BYTES = TILE * TILE * 2;
static constexpr uint32_t MAP_REDRAW_MS = 2500;
static constexpr uint32_t SENSOR_POLL_MS = 250;

static uint16_t rowPixels[TILE];
static uint8_t rowBytes[TILE * 2];
static bool gpsReady=false, imuReady=false, sdReady=false, mapFound=false;
static double latitude=NAN, longitude=NAN;
static float headingDeg=NAN;
static uint32_t lastSensorPoll=0, lastMapDraw=0;
static void drawFooter();

static double radians(double value) { return value * 0.017453292519943295; }

// Convert WGS84 longitude/latitude to fractional XYZ world pixels.
static void worldPixel(double lat, double lon, double &wx, double &wy) {
  const double n = (double)(1UL << MAP_ZOOM) * TILE;
  lat = constrain(lat, -85.05112878, 85.05112878);
  wx = (lon + 180.0) / 360.0 * n;
  const double s = sin(radians(lat));
  wy = (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * PI)) * n;
}

static String tilePath(int x, int y) {
  const int count = 1 << MAP_ZOOM;
  if (y < 0 || y >= count) return String();
  x = ((x % count) + count) % count; // wrap at the antimeridian
  return "/maps/" + String(MAP_ZOOM) + "/" + String(x) + "/" + String(y) + ".rgb565";
}

static bool drawTileAt(int tx, int ty, int screenX, int screenY) {
  const String path = tilePath(tx, ty);
  if (path.isEmpty()) return false;
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file || file.size() < TILE_BYTES) { if (file) file.close(); return false; }

  const int sx0=max(0, -screenX), sx1=min(TILE, SCREEN_W-screenX);
  const int sy0=max(0, -screenY), sy1=min(TILE, SCREEN_H-screenY);
  if (sx0>=sx1 || sy0>=sy1) { file.close(); return false; }
  const size_t bytesToRead=(size_t)(sx1-sx0)*2;

  for (int sy=sy0; sy<sy1; ++sy) {
    const uint32_t offset=((uint32_t)sy*TILE+(uint32_t)sx0)*2;
    if (!file.seek(offset) || file.read(rowBytes,bytesToRead)!=bytesToRead) continue;
    const int count=sx1-sx0;
    for(int i=0;i<count;++i) rowPixels[i]=(uint16_t)rowBytes[2*i] | ((uint16_t)rowBytes[2*i+1]<<8);
    for(int sent=0;sent<count;) {
      const uint8_t chunk=(uint8_t)min(240,count-sent);
      const int px=screenX+sx0+sent;
      tft.setAddrWindow(px,screenY+sy,px+chunk-1,screenY+sy);
      tft.pushColors(rowPixels+sent,chunk,true);
      sent+=chunk;
    }
  }
  file.close();
  return true;
}

static void drawNoFix() {
  tft.fillScreen(0x18E3);
  tft.setTextWrap(false); tft.setTextColor(0xFFFF); tft.setTextSize(2);
  tft.setCursor(10,10); tft.print("GEOpath | OFFLINE MAP");
  tft.drawFastHLine(10,48,300,0x7BEF);
  tft.setTextSize(2); tft.setCursor(20,205); tft.print(gpsReady?"Waiting for GPS fix":"GPS module unavailable");
  tft.setTextSize(1); tft.setCursor(20,235); tft.print("Map tiles load around your current position");
  drawFooter();
}

static void drawFooter() {
  tft.fillRect(0,454,SCREEN_W,26,0x0841);
  tft.drawFastHLine(0,454,SCREEN_W,0x4A69);
  tft.setTextSize(1); tft.setTextColor(0xFFFF); tft.setCursor(8,463);
  tft.print(sdReady?"SD":"NO SD"); tft.print("  ");
  tft.print(mapFound?"MAP TILES":"NO TILES"); tft.print("  ");
  tft.print(gpsReady?"GPS":"NO GPS"); tft.print("  ");
  if(isfinite(headingDeg)) { tft.print("HDG "); tft.print((int)lroundf(headingDeg)); tft.print(" deg"); }
  else tft.print("HDG --");
}

static void drawCompass() {
  // Small north arrow and numeric magnetic heading. Map remains north-up.
  const int cx=SCREEN_W-23, cy=24;
  tft.drawCircle(cx,cy,12,0xFFFF);
  tft.setTextSize(1); tft.setTextColor(0xFFFF); tft.setCursor(cx-3,cy-19); tft.print("N");
  if(!isfinite(headingDeg)) return;
  const double a=radians(headingDeg);
  const int x2=cx+(int)lround(9*sin(a)), y2=cy-(int)lround(9*cos(a));
  tft.drawLine(cx,cy,x2,y2,0x07FF);
  tft.fillCircle(cx,cy,2,0x07FF);
}

static void drawMap() {
  if(!isfinite(latitude) || !isfinite(longitude)) { mapFound=false; drawNoFix(); return; }
  tft.fillScreen(0x39E7); // neutral background for missing map tiles
  double centerX,centerY; worldPixel(latitude,longitude,centerX,centerY);
  const double originX=centerX-SCREEN_W/2.0, originY=centerY-SCREEN_H/2.0;
  const int tx0=(int)floor(originX/TILE), ty0=(int)floor(originY/TILE);
  const int tx1=(int)floor((originX+SCREEN_W-1)/TILE), ty1=(int)floor((originY+SCREEN_H-1)/TILE);
  mapFound=false;
  if(sdReady) for(int ty=ty0;ty<=ty1;++ty) for(int tx=tx0;tx<=tx1;++tx) {
    if(drawTileAt(tx,ty,(int)floor(tx*TILE-originX),(int)floor(ty*TILE-originY))) mapFound=true;
  }
  // Fixed center marker: the map scrolls beneath the current GPS position.
  const int cx=SCREEN_W/2, cy=SCREEN_H/2;
  tft.fillCircle(cx,cy,7,0xF800); tft.drawCircle(cx,cy,9,0xFFFF);
  tft.drawFastVLine(cx,cy-13,5,0xFFFF); tft.drawFastVLine(cx,cy+9,5,0xFFFF);
  tft.drawFastHLine(cx-13,cy,5,0xFFFF); tft.drawFastHLine(cx+9,cy,5,0xFFFF);
  // Top translucent-style information strip.
  tft.fillRect(0,0,SCREEN_W,52,0x0841);
  tft.setTextWrap(false); tft.setTextColor(0xFFFF); tft.setTextSize(1);
  tft.setCursor(9,7); tft.print("GEOpath | OFFLINE MAP");
  tft.setTextColor(0xBDF7); tft.setCursor(9,31);
  tft.printf("%.5f, %.5f  Z%d",latitude,longitude,MAP_ZOOM);
  drawCompass(); drawFooter();
  if(!mapFound) {
    tft.fillRoundRect(8,408,304,28,5,0x0841);
    tft.setTextColor(0xFFE0); tft.setCursor(16,418);
    tft.print(sdReady?"No tile for this location / zoom":"Insert SD with offline map tiles");
  }
}

void setup() {
  Serial.begin(115200); delay(250);
  Wire.begin(Pins::I2C_SDA,Pins::I2C_SCL,400000);
  SPI.begin(Pins::SPI_SCK,Pins::SPI_MISO,Pins::SPI_MOSI);
  pinMode(Pins::SD_CS,OUTPUT); digitalWrite(Pins::SD_CS,HIGH);
  pinMode(Pins::DISPLAY_CS,OUTPUT); digitalWrite(Pins::DISPLAY_CS,HIGH);
  tft.begin(); tft.setRotation(0); tft.fillScreen(0x0000);
  imuReady=imu.begin_I2C(Pins::IMU_ADDRESS,&Wire);
  gpsReady=gps.begin(Wire);
  sdReady=SD.begin(Pins::SD_CS,SPI);
  drawNoFix();
  Serial.printf("Offline map viewer: GPS=%d IMU=%d SD=%d zoom=%u\n",gpsReady,imuReady,sdReady,MAP_ZOOM);
}

void loop() {
  if(millis()-lastSensorPoll>=SENSOR_POLL_MS) {
    lastSensorPoll=millis();
    if(gpsReady && gps.getPVT(50) && gps.getSIV()>=4) {
      latitude=gps.getLatitude()*1e-7;
      longitude=gps.getLongitude()*1e-7;
    }
    if(imuReady) {
      sensors_event_t accel,gyro,mag,temp;
      if(imu.getEvent(&accel,&gyro,&temp,&mag))
        headingDeg=fmodf(atan2f(mag.magnetic.y,mag.magnetic.x)*180.0f/PI+360.0f,360.0f);
    }
  }
  if(millis()-lastMapDraw>=MAP_REDRAW_MS) { lastMapDraw=millis(); drawMap(); }
  delay(5);
}
