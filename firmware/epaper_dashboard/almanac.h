#pragma once
#include "card_schedule.h"
#include "almanac_data.h"

inline int almanacIndex(int year,int month,int day) {
  if(year<ALMANAC_FIRST_YEAR || year>ALMANAC_LAST_YEAR || day<1 || day>calendarMonthDays(year,month))return -1;
  int n=0;
  for(int y=ALMANAC_FIRST_YEAR;y<year;++y)n+=365+(calendarMonthDays(y,2)==29);
  for(int m=1;m<month;++m)n+=calendarMonthDays(year,m);
  return n+day-1;
}
inline uint16_t lunarPacked(int index) {
  return ALMANAC_DAYS[index][0] | (static_cast<uint16_t>(ALMANAC_DAYS[index][1])<<8);
}
