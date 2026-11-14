#pragma once
#include "environment.h"

static String indoorTemperature() {
  if(!environment.ahtValid || !isfinite(environment.temperature))return "--";
  return String(fabs(environment.temperature)<0.05f?0.0f:environment.temperature,1);
}
static String indoorHumidity() {
  return environment.ahtValid && isfinite(environment.humidity)?String(environment.humidity,0)+"%":"--%";
}
static String indoorPressure() {
  return environment.bmpValid && isfinite(environment.pressureHpa)?String(environment.pressureHpa,0)+" hPa":"-- hPa";
}
static String outdoorRange() {
  return weather.valid && weather.dailyValid?String(weather.low,0)+"° / "+String(weather.high,0)+"°":"";
}
static String outdoorUpdate() {
  if(!weather.valid)return "天气待更新";
  String stamp="--:--";
  const String& value=weather.observedAt;
  if(value.length()>=16 && value[13]==':' && value[11]>='0' && value[11]<='2' &&
     value[12]>='0' && value[12]<='9' && value[14]>='0' && value[14]<='5' && value[15]>='0' && value[15]<='9')stamp=value.substring(11,16);
  return String(weatherCached()?"缓存 ":"更新 ")+stamp;
}
// Monochrome 16 px symbols avoid font-dependent emoji on the physical panel.
enum class EnvironmentIcon {Home,Outdoor,Humidity,Pressure};
static void environmentIcon(int x,int y,EnvironmentIcon icon) {
  auto line=[&](int a,int b,int c,int d) {painter->drawLine(x+a,y+b,x+c,y+d,GxEPD_BLACK);};
  switch(icon) {
    case EnvironmentIcon::Home:
      line(0,7,7,0);line(7,0,14,7);line(2,6,2,14);line(12,6,12,14);
      line(2,14,12,14);line(6,14,6,9);line(6,9,9,9);line(9,9,9,14);break;
    case EnvironmentIcon::Outdoor:
      // Sun above a horizon denotes outdoors, not the reported weather.
      painter->drawCircle(x+7,y+7,4,GxEPD_BLACK);
      line(7,0,7,1);line(0,7,1,7);line(13,7,14,7);
      line(1,1,2,2);line(12,2,13,1);line(0,14,14,14);break;
    case EnvironmentIcon::Humidity:
      line(7,0,2,8);line(2,8,2,11);line(2,11,5,14);line(5,14,9,14);
      line(9,14,12,11);line(12,11,12,8);line(12,8,7,0);line(5,10,5,11);break;
    case EnvironmentIcon::Pressure:
      painter->drawCircle(x+7,y+7,7,GxEPD_BLACK);
      line(7,7,11,3);line(7,1,7,2);line(1,7,2,7);line(12,7,13,7);break;
  }
}
static void compactEnvironment(int top) {
  cardRule(top);
  environmentIcon(20,top+7,EnvironmentIcon::Home);
  type(44,top+22,fitText(indoorTemperature()+"°C",102,TextRole::Dense),TextRole::Dense);
  environmentIcon(156,top+7,EnvironmentIcon::Humidity);
  type(180,top+22,indoorHumidity(),TextRole::Dense);
  environmentIcon(258,top+7,EnvironmentIcon::Pressure);
  type(380,top+22,indoorPressure(),TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
  environmentIcon(20,top+30,EnvironmentIcon::Outdoor);
  type(44,top+45,weather.valid?String(weather.temp,0)+"°C":"--°C",TextRole::Dense);
  const String range=outdoorRange();
  const int available=224-(range.isEmpty()?0:textWidth(range,TextRole::Dense)+12);
  const String detail=!weather.valid?String("待更新"):weatherCached()?String("缓存"):weather.text;
  type(156,top+45,fitText(detail,available,TextRole::Dense),TextRole::Dense);
  type(380,top+45,range,TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
}
static void environmentTemperature(int x,int baseline,int width,const String& value) {
  TextRole role=TextRole::Price;
  const int unitWidth=textWidth("°C",TextRole::Body),gap=4;
  if(textWidth(value,role)+unitWidth+gap>width)role=TextRole::Title;
  String fitted=fitText(value,width-unitWidth-gap,role);
  type(x,baseline,fitted,role);
  type(x+textWidth(fitted,role)+gap,baseline-3,"°C",TextRole::Body);
}
static void clockEnvironment() {
  cardRule(167);painter->drawFastVLine(200,182,105,GxEPD_BLACK);
  type(20,192,"室内",TextRole::Dense);
  if(!environment.ahtValid)type(184,192,"未就绪",TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
  const String prefix="室外 · ";
  type(220,192,prefix+fitText(cfg.city,160-textWidth(prefix,TextRole::Dense),TextRole::Dense),TextRole::Dense);
  environmentTemperature(20,243,164,indoorTemperature());
  environmentTemperature(220,243,160,weather.valid?String(weather.temp,0):String("--"));
  type(20,269,"湿度",TextRole::Dense);
  type(184,269,indoorHumidity(),TextRole::Title,GxEPD_BLACK,TextAlign::Right);
  const String range=outdoorRange();
  const int detailWidth=160-(range.isEmpty()?0:textWidth(range,TextRole::Dense)+10);
  type(220,269,fitText(weather.valid?weather.text:String("天气待更新"),max(0,detailWidth),TextRole::Dense),TextRole::Dense);
  type(380,269,range,TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
  type(20,293,"气压 "+indoorPressure(),TextRole::Dense);
  type(380,293,outdoorUpdate(),TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
}
