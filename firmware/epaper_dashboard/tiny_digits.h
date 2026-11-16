#pragma once
#include <Adafruit_GFX.h>

// 5x7 pixel digits at 6px pitch, shared by the battery status readout and
// the history curve extremum labels. Index 10='.', 11='V', 12='-'.
static const uint8_t TINY_DIGIT_GLYPHS[][5]={
  {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},
  {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
  {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},
  {0,0x60,0x60,0,0},{0x1f,0x20,0x40,0x20,0x1f},{8,8,8,8,8}
};
static int tinyGlyphIndex(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c=='.')return 10;
  if(c=='V')return 11;
  if(c=='-')return 12;
  return -1;
}
static int tinyTextWidth(const String& text) {
  int chars=0;for(unsigned i=0;i<text.length();++i)if(tinyGlyphIndex(text[i])>=0)++chars;
  return chars?chars*6-1:0;
}
static void drawTinyText(Adafruit_GFX& canvas,int x,int top,const String& text,uint16_t color) {
  for(unsigned i=0;i<text.length();++i) {
    const int g=tinyGlyphIndex(text[i]);
    if(g<0)continue;
    for(int gx=0;gx<5;++gx)for(int gy=0;gy<7;++gy)
      if(TINY_DIGIT_GLYPHS[g][gx]&(1<<gy))canvas.drawPixel(x+gx,top+gy,color);
    x+=6;
  }
}
