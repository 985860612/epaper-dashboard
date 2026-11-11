#pragma once
#include "battery.h"

// 5x7 status digits fit in the otherwise unused top margin (y=0..10).
// Reuse the framebuffer; no new typeface or red pixels are needed.
static void drawBatteryStatus(Adafruit_GFX& canvas,const BatteryReading& value) {
  constexpr int top=0;
  static const uint8_t glyphs[][5]={
    {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},
    {0,0x60,0x60,0,0},{0x1f,0x20,0x40,0x20,0x1f},{8,8,8,8,8}
  };
  canvas.drawFastHLine(322,top,17,GxEPD_BLACK);canvas.drawFastHLine(322,top+10,17,GxEPD_BLACK);
  canvas.drawFastVLine(322,top,11,GxEPD_BLACK);canvas.drawFastVLine(338,top,11,GxEPD_BLACK);
  canvas.fillRect(339,top+3,2,5,GxEPD_BLACK);
  if(value.charging()) {
    canvas.drawLine(332,top+2,328,top+5,GxEPD_BLACK);canvas.drawFastHLine(328,top+5,5,GxEPD_BLACK);
    canvas.drawLine(332,top+5,328,top+8,GxEPD_BLACK);
  }
  uint16_t cv=value.centivolts();
  uint8_t chars[]={uint8_t(cv/100),10,uint8_t(cv/10%10),uint8_t(cv%10),11};
  if(!value.valid())chars[0]=chars[2]=chars[3]=12;
  for(int i=0;i<5;++i)for(int x=0;x<5;++x)for(int y=0;y<7;++y)
    if(glyphs[chars[i]][x]&(1<<y))canvas.drawPixel(350+i*6+x,top+2+y,GxEPD_BLACK);
}
