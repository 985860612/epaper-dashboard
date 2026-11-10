#pragma once
#include "Arduino.h"
class Adafruit_GFX {
public:
  int16_t WIDTH,HEIGHT;
  Adafruit_GFX(int16_t w,int16_t h):WIDTH(w),HEIGHT(h) {}virtual ~Adafruit_GFX()=default;
  virtual void drawPixel(int16_t x,int16_t y,uint16_t c)=0;
  void fillRect(int x,int y,int w,int h,uint16_t c) {for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)drawPixel(xx,yy,c);}
  void drawFastHLine(int x,int y,int w,uint16_t c) {for(int xx=x;xx<x+w;++xx)drawPixel(xx,y,c);}
  void drawFastVLine(int x,int y,int h,uint16_t c) {for(int yy=y;yy<y+h;++yy)drawPixel(x,yy,c);}
  void drawBitmap(int x,int y,const uint8_t* b,int w,int h,uint16_t c) {int stride=(w+7)/8;for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)if(b[yy*stride+xx/8]&(128>>(xx%8)))drawPixel(x+xx,y+yy,c);}
  void drawLine(int x0,int y0,int x1,int y1,uint16_t c) {int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;for(;;) {drawPixel(x0,y0,c);if(x0==x1 && y0==y1)break;int e=2*err;if(e>=dy) {err+=dy;x0+=sx;}if(e<=dx) {err+=dx;y0+=sy;}}}
  void drawCircle(int cx,int cy,int r,uint16_t c) {int x=r,y=0,err=1-r;while(x>=y) {drawPixel(cx+x,cy+y,c);drawPixel(cx+y,cy+x,c);drawPixel(cx-y,cy+x,c);drawPixel(cx-x,cy+y,c);drawPixel(cx-x,cy-y,c);drawPixel(cx-y,cy-x,c);drawPixel(cx+y,cy-x,c);drawPixel(cx+x,cy-y,c);++y;if(err<0)err+=2*y+1;else {--x;err+=2*(y-x)+1;}}}
};
