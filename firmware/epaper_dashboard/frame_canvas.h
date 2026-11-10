#pragma once
#include <Adafruit_GFX.h>
#include <cstdlib>
#include <cstring>

class FrameCanvas : public Adafruit_GFX {
public:
  static constexpr size_t BYTES=400*300/8;
  uint8_t* black=nullptr;
  uint8_t* red=nullptr;
  FrameCanvas():Adafruit_GFX(400,300) {
    black=static_cast<uint8_t*>(malloc(BYTES)); red=static_cast<uint8_t*>(malloc(BYTES));
    if(black) memset(black,255,BYTES); if(red) memset(red,255,BYTES);
  }
  ~FrameCanvas() { free(black);free(red); }
  bool valid() const { return black && red; }
  void drawPixel(int16_t x,int16_t y,uint16_t color) override {
    if(!valid() || x<0 || x>=400 || y<0 || y>=300)return;
    int i=y*50+x/8;uint8_t bit=0x80>>(x%8);
    black[i]|=bit;red[i]|=bit;
    if(color==0)black[i]&=~bit;
    else if(color==0xf800)red[i]&=~bit;
  }
  bool hasRed(int x,int y,int w,int h) const {
    for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;xx+=8)if(red[yy*50+xx/8]!=255)return true;
    return false;
  }
  uint32_t hash(const uint8_t* data,int x,int y,int w,int h) const {
    uint32_t result=2166136261u;
    for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;xx+=8)result=(result^data[yy*50+xx/8])*16777619u;
    return result;
  }
};
