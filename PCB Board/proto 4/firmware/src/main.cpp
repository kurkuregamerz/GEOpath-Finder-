#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9488.h>
#include <Adafruit_ICM20948.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include "board_pins.h"

// The exact ER-TFT040-5 suffix/strap option must be set for 4-wire SPI.
// This driver expects the ILI9488 module configured for SPI, RGB ordering,
// and compatible 3.3 V logic. The touch panel is the XPT2046 resistive option.
Adafruit_ILI9488 tft(Pins::DISPLAY_CS, Pins::DISPLAY_DC, Pins::DISPLAY_RST);
XPT2046_Touchscreen touch(Pins::TOUCH_CS, Pins::TOUCH_IRQ);
Adafruit_ICM20948 imu;
SFE_UBLOX_GNSS gps;

struct Waypoint { double lat, lon; String name; };
static constexpr size_t MAX_WAYPOINTS = 300;
Waypoint route[MAX_WAYPOINTS];
size_t routeCount = 0, targetIndex = 0;
bool sdReady = false, gpsReady = false, imuReady = false;
double latitude = NAN, longitude = NAN;
float headingDeg = NAN;
uint32_t lastGpsPoll = 0, lastDraw = 0;

static double radians(double d) { return d * 0.017453292519943295; }
static double degrees(double r) { return r * 57.29577951308232; }
static double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
  const double p1 = radians(lat1), p2 = radians(lat2);
  const double dp = radians(lat2 - lat1), dl = radians(lon2 - lon1);
  const double a = sin(dp/2)*sin(dp/2) + cos(p1)*cos(p2)*sin(dl/2)*sin(dl/2);
  return 6371000.0 * 2.0 * atan2(sqrt(a), sqrt(1.0-a));
}
static double bearingTo(double lat1, double lon1, double lat2, double lon2) {
  const double p1 = radians(lat1), p2 = radians(lat2), dl = radians(lon2-lon1);
  const double y = sin(dl)*cos(p2);
  const double x = cos(p1)*sin(p2)-sin(p1)*cos(p2)*cos(dl);
  double b = fmod(degrees(atan2(y,x))+360.0, 360.0);
  return b;
}
static String csvField(const String &line, int index) {
  int start = 0;
  for (int i=0; i<index; ++i) { start=line.indexOf(',', start); if(start<0) return ""; ++start; }
  int end=line.indexOf(',', start); if(end<0) end=line.length();
  String s=line.substring(start,end); s.trim();
  if(s.length()>=2 && s[0]=='"' && s[s.length()-1]=='"') s=s.substring(1,s.length()-1);
  return s;
}
static bool loadRoute(const char *path) {
  File f=SD.open(path, FILE_READ); if(!f) return false;
  while(f.available() && routeCount<MAX_WAYPOINTS) {
    String line=f.readStringUntil('\n'); line.trim();
    if(line.isEmpty() || line[0]=='#') continue;
    String a=csvField(line,0), b=csvField(line,1), n=csvField(line,2);
    char *e1=nullptr,*e2=nullptr;
    double lat=strtod(a.c_str(),&e1), lon=strtod(b.c_str(),&e2);
    if(!e1 || *e1 || !e2 || *e2 || !isfinite(lat) || !isfinite(lon) || fabs(lat)>90 || fabs(lon)>180) continue;
    route[routeCount++]={lat,lon,n.isEmpty()?String("Waypoint ")+String(routeCount+1):n};
  }
  f.close(); return routeCount>0;
}
static void drawStatus(const String &message) {
  tft.fillScreen(0x0000); tft.setTextWrap(false); tft.setTextColor(0xFFFF); tft.setTextSize(2);
  tft.setCursor(12,12); tft.print("GEOpath Finder");
  tft.setTextSize(1); tft.setCursor(12,42);
  tft.printf("GPS:%s  IMU:%s  SD:%s", gpsReady?"OK":"--", imuReady?"OK":"--", sdReady?"OK":"--");
  tft.drawFastHLine(10,58,300,0x7BEF);
  if(!isfinite(latitude)) { tft.setTextSize(2); tft.setCursor(12,76); tft.print("Waiting for GPS fix"); }
  else {
    tft.setTextSize(1); tft.setCursor(12,72); tft.printf("%.6f, %.6f",latitude,longitude);
    if(routeCount && targetIndex<routeCount) {
      const Waypoint &w=route[targetIndex];
      const double d=distanceMeters(latitude,longitude,w.lat,w.lon);
      const double b=bearingTo(latitude,longitude,w.lat,w.lon);
      tft.setTextSize(2); tft.setCursor(12,100); tft.print(w.name.substring(0,22));
      tft.setTextSize(3); tft.setCursor(12,132); tft.printf("%.0f m",d);
      tft.setTextSize(2); tft.setCursor(12,174); tft.printf("Bearing %.0f deg",b);
      tft.setCursor(12,205); if(isfinite(headingDeg)) tft.printf("Heading %.0f deg",headingDeg); else tft.print("Heading unavailable");
      if(isfinite(headingDeg)) {
        double delta=fmod(b-headingDeg+540.0,360.0)-180.0;
        tft.setCursor(12,240); tft.print(fabs(delta)<15?"Go straight":delta>0?"Turn right":"Turn left");
      }
    } else { tft.setTextSize(2); tft.setCursor(12,112); tft.print("No route loaded"); }
  }
  tft.setTextSize(1); tft.setCursor(12,292); tft.print(message.substring(0,50));
}

void setup() {
  Serial.begin(115200); delay(300);
  Wire.begin(Pins::I2C_SDA,Pins::I2C_SCL,400000);
  SPI.begin(Pins::SPI_SCK,Pins::SPI_MISO,Pins::SPI_MOSI);
  pinMode(Pins::SD_CS,OUTPUT); digitalWrite(Pins::SD_CS,HIGH);
  pinMode(Pins::DISPLAY_CS,OUTPUT); digitalWrite(Pins::DISPLAY_CS,HIGH);
  pinMode(Pins::TOUCH_CS,OUTPUT); digitalWrite(Pins::TOUCH_CS,HIGH);
  tft.begin(); tft.setRotation(1); tft.fillScreen(0x0000);
  touch.begin(); touch.setRotation(1);
  imuReady=imu.begin_I2C(Pins::IMU_ADDRESS,&Wire);
  // NEO-M8N exposes DDC (I2C) on SDA/SCL. SparkFun library uses 0x42.
  gpsReady=gps.begin(Wire);
  sdReady=SD.begin(Pins::SD_CS,SPI);
  bool loaded=sdReady && loadRoute("/route.csv");
  drawStatus(loaded?"Route loaded: /route.csv":sdReady?"SD ready; route.csv missing/invalid":"Insert SD with route.csv");
  Serial.printf("Ready: SD=%d GPS=%d IMU=%d route points=%u\n",sdReady,gpsReady,imuReady,(unsigned)routeCount);
}

void loop() {
  if(gpsReady && millis()-lastGpsPoll>=250) {
    lastGpsPoll=millis();
    if(gps.getPVT(50) && gps.getSIV()>=4) {
      latitude=gps.getLatitude()*1e-7; longitude=gps.getLongitude()*1e-7;
    }
  }
  if(imuReady) {
    sensors_event_t accel,gyro,mag,temp;
    if(imu.getEvent(&accel,&gyro,&temp,&mag)) {
      // Tilt-compensated compass needs a level device. Calibrate hard/soft iron
      // offsets for the assembled enclosure before trusting turn guidance.
      headingDeg=(float)fmod(degrees(atan2(mag.magnetic.y,mag.magnetic.x))+360.0,360.0);
    }
  }
  if(isfinite(latitude) && routeCount && targetIndex<routeCount && distanceMeters(latitude,longitude,route[targetIndex].lat,route[targetIndex].lon)<8.0) {
    if(targetIndex+1<routeCount) ++targetIndex;
  }
  if(touch.touched()) {
    TS_Point p=touch.getPoint();
    // Touchscreen calibration depends on the actual glass orientation. Tapping
    // the lower-left screen area advances to the next route point as fallback.
    if(p.x<900 && routeCount && targetIndex+1<routeCount) { ++targetIndex; delay(350); }
  }
  if(millis()-lastDraw>500) { lastDraw=millis(); drawStatus("Tap lower-left to skip waypoint"); }
  delay(10);
}
