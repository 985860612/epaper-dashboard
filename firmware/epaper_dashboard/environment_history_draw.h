#pragma once
#include "environment_history.h"
#include "tiny_digits.h"

static String historyTime(uint32_t epoch) {
  time_t value=epoch;tm local{};localtime_r(&value,&local);return cardTime(local);
}
static void drawEnvironmentHistory(const tm& now) {
  tm copy=now;const int64_t epoch=mktime(&copy);
  const uint32_t end=EnvironmentHistory::bucket(epoch);
  const auto* latest=environmentHistory.latest(epoch);
  cardHeader(cardDate(now),cardTime(now));
  const String temperature=latest && latest->hasTemperature()?String(latest->temperature/10.0f,1):String("--");
  type(20,103,temperature,TextRole::Price,GxEPD_RED);
  type(26+textWidth(temperature,TextRole::Price),100,"°C",TextRole::Body,GxEPD_RED);
  type(374-textWidth("%",TextRole::Body),103,latest && latest->hasHumidity()?String(latest->humidity/10.0f,0):String("--"),TextRole::Price,GxEPD_BLACK,TextAlign::Right);
  type(380,100,"%",TextRole::Body,GxEPD_BLACK,TextAlign::Right);

  constexpr int left=48,right=352,top=135,bottom=253;
  float tLow=100,tHigh=-100,hLow=100,hHigh=0;bool hasT=false,hasH=false;
  if(end)for(int i=0;i<ENV_HISTORY_CAPACITY;++i)if(const auto* p=environmentHistory.at(end-uint32_t(i))) {
    if(p->epoch>epoch)continue;
    if(p->hasTemperature()) {float t=p->temperature/10.0f;tLow=min(tLow,t);tHigh=max(tHigh,t);hasT=true;}
    if(p->hasHumidity()) {float h=p->humidity/10.0f;hLow=min(hLow,h);hHigh=max(hHigh,h);hasH=true;}
  }
  if(!hasT) {tLow=20;tHigh=30;}
  else {tLow=floorf(tLow);tHigh=ceilf(tHigh);if(tHigh-tLow<4) {tLow=floorf((tLow+tHigh)/2-2);tHigh=tLow+4;}}
  if(!hasH) {hLow=0;hHigh=100;}
  else {hLow=floorf(hLow/10)*10;hHigh=ceilf(hHigh/10)*10;if(hHigh<=hLow) {hLow=max(0.f,hLow-10);hHigh=min(100.f,hLow+10);}}
  auto y=[&](float v,float low,float high) {return bottom-int(lroundf((v-low)/(high-low)*(bottom-top)));};
  for(int yy:{top,(top+bottom)/2,bottom})for(int x=left;x<=right;x+=8)painter->drawPixel(x,yy,GxEPD_BLACK);
  type(40,top+6,String(tHigh,0),TextRole::Dense,GxEPD_RED,TextAlign::Right);
  type(40,bottom+6,String(tLow,0),TextRole::Dense,GxEPD_RED,TextAlign::Right);
  type(360,top+6,String(hHigh,0),TextRole::Dense);
  type(360,bottom+6,String(hLow,0),TextRole::Dense);
  // Only connect adjacent recorded buckets. Missing/invalid samples remain gaps.
  // No per-point markers: dense dots thicken the line. Mark only each line's
  // highest and lowest point instead.
  for(bool temperatureLine:{false,true}) {
    bool previousValid=false;int px=0,py=0;
    bool hasPoint=false;float minValue=0,maxValue=0;int minX=0,minY=0,maxX=0,maxY=0;
    if(end)for(int i=0;i<ENV_HISTORY_CAPACITY;++i) {
      const auto* p=environmentHistory.at(end-(ENV_HISTORY_CAPACITY-1-uint32_t(i)));
      const bool valid=p && p->epoch<=epoch && (temperatureLine?p->hasTemperature():p->hasHumidity());
      if(valid) {
        const int x=left+i*(right-left)/(ENV_HISTORY_CAPACITY-1);
        const float value=temperatureLine?p->temperature/10.0f:p->humidity/10.0f;
        const int yy=temperatureLine?y(value,tLow,tHigh):y(value,hLow,hHigh);
        if(previousValid)painter->drawLine(px,py,x,yy,temperatureLine?GxEPD_RED:GxEPD_BLACK);
        if(!hasPoint||value<minValue){minValue=value;minX=x;minY=yy;}
        if(!hasPoint||value>maxValue){maxValue=value;maxX=x;maxY=yy;}
        hasPoint=true;px=x;py=yy;
      }
      previousValid=valid;
    }
    if(hasPoint&&maxValue>minValue) {
      const uint16_t ink=temperatureLine?GxEPD_RED:GxEPD_BLACK;
      painter->fillRect(maxX-1,maxY-1,3,3,ink);
      // Tiny 5x7 labels match the battery voltage readout. Max label above the
      // marker, min label below; flip sides when the marker hugs the plot edge.
      auto label=[&](int cx,int cy,const String& text,bool above) {
        const int half=tinyTextWidth(text)/2;
        cx=max(left+half,min(right-half,cx));
        int textTop=above?cy-11:cy+4;
        if(above&&textTop<top)textTop=cy+4;
        if(!above&&textTop+7>bottom+12)textTop=cy-11;
        drawTinyText(*painter,cx-half,textTop,text,ink);
      };
      const bool decimals=temperatureLine;
      label(maxX,maxY,String(maxValue,decimals?1:0),true);
      if(minX!=maxX||minY!=maxY) {
        painter->fillRect(minX-1,minY-1,3,3,ink);
        label(minX,minY,String(minValue,decimals?1:0),false);
      }
    }
  }
  if(!hasT && !hasH)type(200,200,"等待数据",TextRole::Body,GxEPD_BLACK,TextAlign::Center);
  if(end)for(int i:{0,48,96,143}) {
    const int x=left+i*(right-left)/(ENV_HISTORY_CAPACITY-1);
    type(x,281,historyTime((end-(143-uint32_t(i)))*ENV_HISTORY_INTERVAL),TextRole::Dense,GxEPD_BLACK,TextAlign::Center);
  }
  if(environmentHistoryError[0])type(200,298,"数据保存失败",TextRole::Dense,GxEPD_BLACK,TextAlign::Center);
}
