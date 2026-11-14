#pragma once
#include "almanac.h"
static bool cardDrawingFailed=false;
static String cardTime(const tm& now) {char s[6];snprintf(s,sizeof(s),"%02d:%02d",now.tm_hour,now.tm_min);return s;}
static String cardDate(const tm& now) {char s[16];snprintf(s,sizeof(s),"%04d.%02d.%02d",now.tm_year+1900,now.tm_mon+1,now.tm_mday);return s;}
static String cardDay(const tm& now) {return String(now.tm_mday<10?"0":"")+String(now.tm_mday);}
static String cardMonth(const tm& now) {return String(now.tm_year+1900)+" 年  "+String(now.tm_mon+1)+" 月";}
static const char* const CARD_WEEKDAYS[]={"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
static void cardRule(int y) {painter->drawFastHLine(20,y,360,GxEPD_BLACK);}
static void cardHeader(const String& title,const String& detail,TextRole role=TextRole::Body) {
  painter->fillRect(20,21,4,18,GxEPD_RED);
  int gap=detail.isEmpty()?0:textWidth(detail,TextRole::Body)+16;
  String heading=fitText(title,348-gap,role);type(32,38,heading,role);type(380,38,detail,TextRole::Body,GxEPD_BLACK,TextAlign::Right);
  if(clockReady() && currentPhase()==Phase::Unknown) {
    int left=44+textWidth(heading,role),right=detail.isEmpty()?288:368-textWidth(detail);String notice="日历待更新";
    if(right-left>=textWidth(notice,TextRole::Dense))type((left+right)/2,38,notice,TextRole::Dense,GxEPD_RED,TextAlign::Center);
  }cardRule(51);
}
static void cardCloud(int x,int y) {
  painter->drawCircle(x+14,y+20,10,GxEPD_BLACK);painter->drawCircle(x+27,y+15,13,GxEPD_BLACK);painter->drawCircle(x+41,y+21,10,GxEPD_BLACK);
  painter->fillRect(x+10,y+18,35,13,GxEPD_WHITE);painter->drawFastHLine(x+10,y+30,35,GxEPD_BLACK);
}
static bool weatherCached() {return !weatherError.isEmpty() || (weather.valid && time(nullptr)-weather.receivedAt>3600);}
static String weatherDescription() {
  if(!weather.valid)return "天气待更新";
  return weather.text+(weatherCached()?"·缓存":"");
}
#include "environment_card.h"
static void cardWeatherFooter() {compactEnvironment(248);}
static void drawBigCalendar(const tm& now) {
  cardHeader(cardMonth(now),"");
  type(380,38,cardTime(now),TextRole::Title,GxEPD_BLACK,TextAlign::Right);
  type(380,81,CARD_WEEKDAYS[now.tm_wday],TextRole::Body,GxEPD_BLACK,TextAlign::Right);
  type(200,229,cardDay(now),TextRole::Day,GxEPD_RED,TextAlign::Center);cardWeatherFooter();
}
static void drawMonthWeather(const tm& now) {
  cardHeader(cardMonth(now),"");type(380,38,cardTime(now),TextRole::Title,GxEPD_BLACK,TextAlign::Right);
  type(66,159,cardDay(now),TextRole::CompactDate,GxEPD_RED,TextAlign::Center);
  type(66,190,CARD_WEEKDAYS[now.tm_wday],TextRole::Body,GxEPD_BLACK,TextAlign::Center);
  type(66,224,cardTime(now),TextRole::Title,GxEPD_BLACK,TextAlign::Center);painter->drawFastVLine(126,70,165,GxEPD_BLACK);
  const char* weekdays[]={"日","一","二","三","四","五","六"};
  for(int i=0;i<7;++i)type(151+i*34,88,weekdays[i],TextRole::Dense,i==0||i==6?GxEPD_RED:GxEPD_BLACK,TextAlign::Center);
  int first=(now.tm_wday-(now.tm_mday-1)%7+7)%7,days=calendarMonthDays(now.tm_year+1900,now.tm_mon+1);
  int step=(first+days+6)/7==6?22:25;
  for(int day=1;day<=days;++day) {
    int slot=first+day-1,col=slot%7,row=slot/7,x=151+col*34,y=119+row*step;uint16_t color=col==0||col==6?GxEPD_RED:GxEPD_BLACK;
    if(day==now.tm_mday) {painter->fillRect(x-13,y-18,27,23,GxEPD_RED);color=GxEPD_WHITE;}
    type(x,y,String(day),TextRole::Body,color,TextAlign::Center);
  }
  cardWeatherFooter();
}
static void drawAlmanac(const tm& now) {
  cardHeader("黄历",cardDate(now)+" "+cardTime(now),TextRole::Title);
  if(currentPhase()!=Phase::Unknown)type(100,38,"民俗参考",TextRole::Dense);
  type(20,146,cardDay(now),TextRole::CompactDate,GxEPD_RED);
  int index=almanacIndex(now.tm_year+1900,now.tm_mon+1,now.tm_mday);
  if(index<0) {type(146,104,"黄历数据待更新",TextRole::Body);type(146,139,"支持2026至2028年",TextRole::Dense);}
  else {
    uint16_t packed=lunarPacked(index);int month=(packed>>5)&15,day=packed&31,gz=packed>>10;
    const char* months[]={"正","二","三","四","五","六","七","八","九","十","冬","腊"};
    const char* days[]={"初一","初二","初三","初四","初五","初六","初七","初八","初九","初十","十一","十二","十三","十四","十五","十六","十七","十八","十九","二十","廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"};
    const char* gan[]={"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};const char* zhi[]={"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
    type(146,104,fitText(String("农历")+((packed&512)?"闰":"")+months[month]+"月"+days[day],234,TextRole::Title),TextRole::Title);
    type(146,139,String(gan[gz%10])+zhi[gz%12]+"年");type(380,139,CARD_WEEKDAYS[now.tm_wday],TextRole::Body,GxEPD_BLACK,TextAlign::Right);
    for(int row=0;row<2;++row) {
      int y=190+row*36;uint16_t ink=row?GxEPD_BLACK:GxEPD_RED;painter->fillRect(20,y-22,32,31,ink);type(26,y+1,row?"忌":"宜",TextRole::Body,GxEPD_WHITE);
      int a=ALMANAC_DAYS[index][2+row*2],b=ALMANAC_DAYS[index][3+row*2];type(70,y,fitText(String(ALMANAC_WORDS[a])+(b?String(" ")+ALMANAC_WORDS[b]:""),310));
    }
  }
  cardRule(166);compactEnvironment(248);
}
static void drawMinimalClock(const tm& now) {
  cardHeader(cardDate(now),CARD_WEEKDAYS[now.tm_wday]);
  type(200,148,cardTime(now),TextRole::Clock,GxEPD_BLACK,TextAlign::Center);
  clockEnvironment();
}
static void drawOverview(const tm& now) {
  cardHeader("今日总览",cardMonth(now),TextRole::Title);
  type(20,147,cardDay(now),TextRole::CompactDate,GxEPD_RED);painter->drawFastVLine(146,72,88,GxEPD_BLACK);
  type(380,121,cardTime(now),TextRole::Value,GxEPD_BLACK,TextAlign::Right);type(380,153,CARD_WEEKDAYS[now.tm_wday],TextRole::Body,GxEPD_BLACK,TextAlign::Right);
  compactEnvironment(176);
  cardRule(230);type(20,256,fitText(stock.valid?stock.name:cfg.stocks[0],195));
  String stamp=stock.valid?stock.stamp.substring(4,6)+"."+stock.stamp.substring(6,8)+" "+stock.stamp.substring(8,10)+":"+stock.stamp.substring(10,12):"行情待更新";
  if(stock.valid && stock.stamp.substring(0,8)!=dateCompact())stamp="历史行情";else if(!stockError.isEmpty())stamp="行情缓存";
  type(380,256,fitText(stamp,160,TextRole::Dense),TextRole::Dense,GxEPD_BLACK,TextAlign::Right);
  type(20,287,stock.valid?String(stock.price,2):"--.--",TextRole::Title); // Secondary quote stays black for fast B/W updates.
  String change=stock.valid?(stock.change>0?"+":"")+String(stock.percent,2)+"%":"--%";type(380,287,change,TextRole::Body,GxEPD_BLACK,TextAlign::Right);
  if(stock.valid && stock.count>1) {
    float lo=stock.points[0].price,hi=lo;for(int i=1;i<stock.count;++i) {lo=min(lo,stock.points[i].price);hi=max(hi,stock.points[i].price);}if(hi<=lo)hi=lo+.01f;
    for(int i=1;i<stock.count;++i)painter->drawLine(153+stock.points[i-1].offset*105/240,286-(stock.points[i-1].price-lo)*24/(hi-lo),153+stock.points[i].offset*105/240,286-(stock.points[i].price-lo)*24/(hi-lo),GxEPD_BLACK);
  }
}
#include "environment_history_draw.h"
static void drawCard(CardId id,const tm& now) {
  cardDrawingFailed=false;switch(id) {case CardId::BigCalendar:drawBigCalendar(now);break;case CardId::MonthWeather:drawMonthWeather(now);break;case CardId::Almanac:drawAlmanac(now);break;case CardId::MinimalClock:drawMinimalClock(now);break;case CardId::Overview:drawOverview(now);break;case CardId::EnvironmentHistory:drawEnvironmentHistory(now);break;}
}
