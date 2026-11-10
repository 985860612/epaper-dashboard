#pragma once
// Compatibility for external diagnostics; normal pages share typography.h.
#include "typography.h"
class NativeText {
  Adafruit_GFX* target=nullptr;int x=0,y=0;uint16_t color=0;
public:
  void begin(Adafruit_GFX& d) {target=&d;}
  void setForegroundColor(uint16_t c) {color=c;}
  void setCursor(int xx,int yy) {x=xx;y=yy;}
  void setScale(uint8_t) {}
  int getUTF8Width(const char* p) {return textWidth(String(p));}
  void print(const String& value) {if(target)drawText(*target,x,y,value,TextRole::Body,color);x+=textWidth(value);}
};
