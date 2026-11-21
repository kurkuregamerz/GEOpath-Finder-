#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>

// Wokwi stand-in firmware: ILI9341 represents the portrait ILI9488 panel;
// fixed coordinates and heading stand in for the NEO-M8N and ICM-20948.
static constexpr int W=240, H=320, TILE=256;
static constexpr uint8_t ZOOM=16;
static constexpr int TFT_CS=40, TFT_DC=39, SD_CS=18;
static constexpr double MOCK_LAT=37.4220, MOCK_LON=-122.0840;
static constexpr int MOCK_HEADING=35;
Adafruit_ILI9341 tft(TFT_CS,TFT_DC);
uint16_t pixels[TILE];
uint8_t bytes[TILE*2];
bool sdOk=false, anyTile=false;

static double rad(double d){return d*0.017453292519943295;}
static void world(double lat,double lon,double &x,double &y){
  const double n=(double)(1UL<<ZOOM)*TILE;
  double s=sin(rad(constrain(lat,-85.05112878,85.05112878)));
  x=(lon+180.0)/360.0*n;
  y=(0.5-log((1+s)/(1-s))/(4.0*PI))*n;
}
static String pathFor(int x,int y){
  const int n=1<<ZOOM; if(y<0||y>=n)return "";x=((x%n)+n)%n;
  return "/maps/"+String(ZOOM)+"/"+String(x)+"/"+String(y)+".rgb565";
}
static void paintTile(int tx,int ty,int sx,int sy){
  String path=pathFor(tx,ty);if(path.isEmpty())return;
  File f=SD.open(path.c_str(),FILE_READ);if(!f||f.size()<TILE*TILE*2){if(f)f.close();return;}
  int x0=max(0,-sx),x1=min(TILE,W-sx),y0=max(0,-sy),y1=min(TILE,H-sy);
  for(int y=y0;y<y1;++y){
    size_t count=(size_t)(x1-x0)*2;
    if(!f.seek(((uint32_t)y*TILE+x0)*2)||f.read(bytes,count)!=(int)count)continue;
    int n=x1-x0;for(int i=0;i<n;++i)pixels[i]=(uint16_t)bytes[2*i]|((uint16_t)bytes[2*i+1]<<8);
    tft.drawRGBBitmap(sx+x0,sy+y,pixels,n,1);
  }
  f.close();anyTile=true;
}
static void draw(){
  tft.fillScreen(0x39E7);anyTile=false;
  double cx,cy;world(MOCK_LAT,MOCK_LON,cx,cy);
  double ox=cx-W/2.0,oy=cy-H/2.0;
  int x0=floor(ox/TILE),y0=floor(oy/TILE),x1=floor((ox+W-1)/TILE),y1=floor((oy+H-1)/TILE);
  if(sdOk)for(int ty=y0;ty<=y1;++ty)for(int tx=x0;tx<=x1;++tx)
    paintTile(tx,ty,floor(tx*TILE-ox),floor(ty*TILE-oy));
  tft.fillRect(0,0,W,48,0x0841);
  tft.setTextWrap(false);tft.setTextSize(1);tft.setTextColor(0x07FF);tft.setCursor(7,7);tft.print("GEOpath | OFFLINE MAP");
  tft.setTextColor(0xFFFF);tft.setCursor(7,24);tft.printf("%.5f, %.5f Z%d",MOCK_LAT,MOCK_LON,ZOOM);
  int mx=W/2,my=H/2;tft.fillCircle(mx,my,6,0xF800);tft.drawCircle(mx,my,9,0xFFFF);
  tft.drawFastVLine(mx,my-14,5,0xFFFF);tft.drawFastVLine(mx,my+9,5,0xFFFF);
  tft.drawFastHLine(mx-14,my,5,0xFFFF);tft.drawFastHLine(mx+9,my,5,0xFFFF);
  int hx=W-20,hy=20;tft.drawCircle(hx,hy,10,0xFFFF);tft.setCursor(hx-2,hy-17);tft.print("N");
  double a=rad(MOCK_HEADING);tft.drawLine(hx,hy,hx+8*sin(a),hy-8*cos(a),0x07FF);
  tft.fillRect(0,H-22,W,22,0x0841);tft.setCursor(7,H-15);tft.setTextColor(0xFFFF);
  tft.print(sdOk?"SD OK":"NO SD");tft.print("  ");tft.print(anyTile?"MAP OK":"NO TILES");tft.print("  GPS SIM  HDG 35");
  if(!anyTile){tft.fillRoundRect(12,H-52,W-24,21,4,0x0841);tft.setCursor(19,H-45);tft.setTextColor(0xFFE0);tft.print("Put demo tiles in sdcard/maps");}
}
void setup(){
  Serial.begin(115200);SPI.begin(12,13,11);
  pinMode(SD_CS,OUTPUT);digitalWrite(SD_CS,HIGH);pinMode(TFT_CS,OUTPUT);digitalWrite(TFT_CS,HIGH);
  tft.begin();tft.setRotation(0);sdOk=SD.begin(SD_CS,SPI);draw();
  Serial.printf("Wokwi preview: SD=%d, fixed GPS/heading, tiles=%d\n",sdOk,anyTile);
}
void loop(){delay(1000);}
