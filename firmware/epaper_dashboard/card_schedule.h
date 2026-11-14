#pragma once
#include <cstdint>
#include <cstring>

enum class CardId : uint8_t { BigCalendar, MonthWeather, Almanac, MinimalClock, Overview, EnvironmentHistory };
struct CardChoice { const char* code; const char* name; };
constexpr CardChoice CARD_CHOICES[] = {
  {"big_calendar","大字日历"}, {"month_weather","月历天气"},
  {"almanac","黄历"}, {"minimal_clock","极简时钟"}, {"overview","今日总览"},
  {"environment_history","温湿度曲线"}
};
constexpr int CARD_COUNT=sizeof(CARD_CHOICES)/sizeof(CARD_CHOICES[0]);
inline int cardIndex(const char* code) {
  for(int i=0;i<CARD_COUNT;++i)if(std::strcmp(code,CARD_CHOICES[i].code)==0)return i;
  return -1;
}
constexpr bool cardRotationDue(uint32_t now,uint32_t started,uint32_t seconds) {
  return static_cast<uint32_t>(now-started)>=seconds*1000UL;
}
constexpr uint8_t nextCardIndex(uint8_t index) { return (index+1)%CARD_COUNT; }
constexpr bool stockTakesPriority(bool explicitStock,bool autoView,bool fixedCard,bool tradingPhase) {
  return explicitStock || (autoView && !fixedCard && tradingPhase);
}
constexpr int calendarMonthDays(int year,int month) {
  return month<1 || month>12 ? 0 : month==2 ? 28+(year%4==0 && (year%100!=0 || year%400==0)) :
    (month==4 || month==6 || month==9 || month==11 ? 30 : 31);
}
