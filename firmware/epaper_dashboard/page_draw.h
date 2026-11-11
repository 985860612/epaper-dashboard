#pragma once
static void type(int x,int y,const String& value,TextRole role=TextRole::Body,uint16_t color=GxEPD_BLACK,TextAlign align=TextAlign::Left) {drawText(*painter,x,y,value,role,color,align);}
static void label(int x,int y,const String& value,uint16_t color=GxEPD_BLACK,bool right=false) {type(x,y,value,TextRole::Body,color,right?TextAlign::Right:TextAlign::Left);}

static String phaseLabel(Phase p) {
  switch (p) {
    case Phase::Auction: return "集合竞价";
    case Phase::Opening: return "待开盘";
    case Phase::Trading: return "交易中";
    case Phase::Closing: return "收盘竞价";
    case Phase::Unknown: return "日历待更新";
    default: return "已休市";
  }
}

static void drawClock(const tm& now,bool valid,const Weather& w,const String& city,Phase phase);
static void drawStock(const tm& now,const Stock& s,const Weather& w,const String& city,Phase phase,int index,bool stale);

#include "cards_draw.h"
#include "font_test_draw.h"

static void drawClock(const tm& now,bool valid,const Weather&,const String&,Phase phase) {
  cardHeader(valid?cardDate(now):"等待阿里云校时",valid?String(CARD_WEEKDAYS[now.tm_wday]):String(""));
  type(200,177,valid?cardTime(now):String("--:--"),TextRole::Clock,GxEPD_BLACK,TextAlign::Center);cardRule(208);
  cardCloud(20,232);type(82,274,weather.valid?String(weather.temp,0):String("--"),TextRole::Value);
  type(380,243,fitText(weatherDescription(),160),TextRole::Body,GxEPD_BLACK,TextAlign::Right);type(380,277,fitText(cfg.city,180),TextRole::Body,GxEPD_BLACK,TextAlign::Right);
  if(valid && phase==Phase::Unknown)type(20,76,"请更新交易日历",TextRole::Dense,GxEPD_RED);
}
static constexpr TextRole STOCK_AXIS_TEXT=TextRole::Body;
static String axisNumber(float value,int width) {
  int decimals=fabs(value)>=1000?0:2;String s;
  do {s=String(value,decimals);if(textWidth(s,STOCK_AXIS_TEXT)<=width)return s;}while(--decimals>=0);
  float unit=fabs(value)>=1000000?1000000:1000;return String(value/unit,1)+(unit==1000000?"M":"k");
}
static String stockRate(float pct) {return isfinite(pct)?String(fabs(pct),2)+"%":String("--%");}
static void stockRow(int y,const String& price,float pct,int width,uint16_t ink=GxEPD_BLACK,int left=20) {
  int right=366-width;
  type(right,y,fitText(price,min(90,max(0,right-left)),TextRole::Body),TextRole::Body,ink,TextAlign::Right);
  type(right+6,y,"/",TextRole::Body,ink);
  type(right+18,y,pct>0?"+":pct<0?"-":"",TextRole::Body,ink);
  type(396,y,fitText(stockRate(pct),88,TextRole::Body),TextRole::Body,ink,TextAlign::Right);
}
static void stockSegment(int x0,int y0,float p0,int x1,int y1,float p1,float reference,int referenceY) {
  uint16_t c0=!renderBw && p0>reference?GxEPD_RED:GxEPD_BLACK;
  uint16_t c1=!renderBw && p1>reference?GxEPD_RED:GxEPD_BLACK;
  if(c0==c1) {painter->drawLine(x0,y0,x1,y1,c0);return;}
  int cross=x0+static_cast<int>(lround((reference-p0)/(p1-p0)*(x1-x0)));
  painter->drawLine(x0,y0,cross,referenceY,c0);
  painter->drawLine(cross,referenceY,x1,y1,c1);
  painter->drawPixel(cross,referenceY,GxEPD_BLACK);
}
static void drawStock(const tm& now,const Stock& s,const Weather& w,const String& city,Phase phase,int index,bool stale) {
  char date[24];snprintf(date,sizeof(date),"%02d.%02d  %02d:%02d",now.tm_mon+1,now.tm_mday,now.tm_hour,now.tm_min);
  type(20,29,date);String temp=w.valid?String(w.temp,0)+"°C":"--";
  type(380,29,fitText(city,148-textWidth(temp))+" "+temp,TextRole::Body,GxEPD_BLACK,TextAlign::Right);cardRule(42);
  bool historical=s.valid && s.stamp.substring(0,8)!=dateCompact();
  String name=fitText(s.valid?s.name:cfg.stocks[index],historical?188:250,TextRole::StockName);type(20,77,name,TextRole::StockName);
  if(historical && s.minuteDate.length()==8)type(28+textWidth(name,TextRole::StockName),77,s.minuteDate.substring(4,6)+"."+s.minuteDate.substring(6,8),TextRole::Dense);
  String state=historical?"历史行情":stale?"数据延迟":phaseLabel(phase);
  type(380,73,state,TextRole::Body,GxEPD_BLACK,TextAlign::Right);
  float change=s.price-s.previous,percent=s.previous>0?change/s.previous*100:0;uint16_t ink=!renderBw && change>0?GxEPD_RED:GxEPD_BLACK;
  String quote=s.valid?String(s.price,2):String("--.--");
  type(20,124,quote,TextRole::Price,renderBw?GxEPD_BLACK:GxEPD_RED);
  bool auction=(phase==Phase::Auction || phase==Phase::Opening) && !historical;
  bool chart=s.valid && s.count>0 && s.previous>0 && !auction;
  float lo=s.previous,hi=s.previous;
  if(chart) {
    for(int i=0;i<s.count;++i) {lo=min(lo,s.points[i].price);hi=max(hi,s.points[i].price);}
    float margin=hi>lo?(hi-lo)*.03f:max(.01f,s.previous*.0005f);lo-=margin;hi+=margin;
  }
  int width=min(88,textWidth(stockRate(percent)));
  stockRow(112,s.valid?(change>0?"+":"")+String(change,2):String("--"),s.valid?percent:NAN,width,ink,20+textWidth(quote,TextRole::Price)+16);
  if(chart) {
    constexpr int left=20,right=396,top=132,bottom=252;
    auto py=[&](float value) {return bottom-static_cast<int>(lround((value-lo)/(hi-lo)*(bottom-top)));};
    for(int x=left;x<=right;x+=8)painter->drawFastHLine(x,py(s.previous),min(4,right-x+1),GxEPD_BLACK);
    for(int i=1;i<s.count;++i) {
      int x0=left+s.points[i-1].offset*(right-left)/240,x1=left+s.points[i].offset*(right-left)/240;
      stockSegment(x0,py(s.points[i-1].price),s.points[i-1].price,x1,py(s.points[i].price),s.points[i].price,s.previous,py(s.previous));
    }
    // Range labels belong to the plot bounds, not the previous-close line.
    // Keep glyphs above/below the curve so they never chase it to the bottom.
    type(right,top,axisNumber(hi,90),TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
    type(right,bottom+22,axisNumber(lo,90),TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
  } else {type(30,184,fitText(auction?"集合竞价·等待正式开盘":"等待有效分时数据",340));type(30,215,fitText(stockError.length()?stockError:"分时线从09:30开始绘制",340),TextRole::Dense);}
}

