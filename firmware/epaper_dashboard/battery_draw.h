#pragma once
#include "battery.h"
#include "tiny_digits.h"

// 5x7 status digits fit in the otherwise unused top margin (y=0..10).
// Reuse the framebuffer; no new typeface or red pixels are needed.
static void drawBatteryStatus(Adafruit_GFX& canvas,const BatteryReading& value) {
  constexpr int top=0;
  canvas.drawFastHLine(322,top,17,GxEPD_BLACK);canvas.drawFastHLine(322,top+10,17,GxEPD_BLACK);
  canvas.drawFastVLine(322,top,11,GxEPD_BLACK);canvas.drawFastVLine(338,top,11,GxEPD_BLACK);
  canvas.fillRect(339,top+3,2,5,GxEPD_BLACK);
  if(value.charging()) {
    canvas.drawLine(332,top+2,328,top+5,GxEPD_BLACK);canvas.drawFastHLine(328,top+5,5,GxEPD_BLACK);
    canvas.drawLine(332,top+5,328,top+8,GxEPD_BLACK);
  } else for(uint8_t i=0;i<value.bars();++i)canvas.fillRect(325+i*3,top+2,2,7,GxEPD_BLACK);
  uint16_t cv=value.centivolts();
  uint8_t chars[]={uint8_t(cv/100),10,uint8_t(cv/10%10),uint8_t(cv%10),11};
  if(!value.valid())chars[0]=chars[2]=chars[3]=12;
  for(int i=0;i<5;++i)for(int x=0;x<5;++x)for(int y=0;y<7;++y)
    if(TINY_DIGIT_GLYPHS[chars[i]][x]&(1<<y))canvas.drawPixel(350+i*6+x,top+2+y,GxEPD_BLACK);
}
