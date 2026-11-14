#pragma once
#include "environment_history.h"

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
  for(bool temperatureLine:{false,true}) {
    bool previousValid=false;int px=0,py=0;
    if(end)for(int i=0;i<ENV_HISTORY_CAPACITY;++i) {
      const auto* p=environmentHistory.at(end-(ENV_HISTORY_CAPACITY-1-uint32_t(i)));
      const bool valid=p && p->epoch<=epoch && (temperatureLine?p->hasTemperature():p->hasHumidity());
      if(valid) {
        const int x=left+i*(right-left)/(ENV_HISTORY_CAPACITY-1);
        const int yy=temperatureLine?y(p->temperature/10.0f,tLow,tHigh):y(p->humidity/10.0f,hLow,hHigh);
        const uint16_t ink=temperatureLine?GxEPD_RED:GxEPD_BLACK;
        if(previousValid)painter->drawLine(px,py,x,yy,ink);
        painter->fillRect(x-1,yy-1,3,3,ink);px=x;py=yy;
      }
      previousValid=valid;
    }
  }
  if(!hasT && !hasH)type(200,200,"等待数据",TextRole::Body,GxEPD_BLACK,TextAlign::Center);
  if(end)for(int i:{0,48,96,143}) {
    const int x=left+i*(right-left)/(ENV_HISTORY_CAPACITY-1);
    type(x,281,historyTime((end-(143-uint32_t(i)))*ENV_HISTORY_INTERVAL),TextRole::Dense,GxEPD_BLACK,TextAlign::Center);
  }
  if(environmentHistoryError[0])type(200,298,"数据保存失败",TextRole::Dense,GxEPD_BLACK,TextAlign::Center);
}
