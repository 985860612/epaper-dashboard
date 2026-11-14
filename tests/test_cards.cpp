#include "../firmware/epaper_dashboard/card_schedule.h"
#include "../firmware/epaper_dashboard/almanac.h"
#include <cassert>
#include <iostream>

int main() {
  static_assert(CARD_COUNT==6);
  assert(cardIndex("environment_history")==static_cast<int>(CardId::EnvironmentHistory));
  for(int i=0;i<CARD_COUNT;++i)assert(cardIndex(CARD_CHOICES[i].code)==i);
  assert(cardIndex("unknown")==-1);
  uint8_t current=0;
  for(int i=0;i<30;++i) { assert(current==i%CARD_COUNT);current=nextCardIndex(current); }
  for(uint32_t start:{0u,10000u,0xfffffff0u}) {
    assert(!cardRotationDue(static_cast<uint32_t>(start+599999u),start,600));
    assert(cardRotationDue(static_cast<uint32_t>(start+600000u),start,600));
  }
  assert(stockTakesPriority(false,true,false,true));
  assert(!stockTakesPriority(false,true,false,false));
  assert(!stockTakesPriority(false,true,true,true));
  assert(!stockTakesPriority(false,false,false,true));
  assert(stockTakesPriority(true,false,true,false));
  assert(calendarMonthDays(2028,2)==29 && calendarMonthDays(2027,2)==28);
  assert(calendarMonthDays(1900,2)==28 && calendarMonthDays(2000,2)==29);
  assert(almanacIndex(2025,12,31)==-1 && almanacIndex(2029,1,1)==-1);
  assert(almanacIndex(2028,2,30)==-1 && almanacIndex(2026,13,1)==-1);
  int expected=0;
  for(int y=2026;y<=2028;++y)for(int m=1;m<=12;++m)for(int d=1;d<=calendarMonthDays(y,m);++d) {
    int index=almanacIndex(y,m,d);assert(index==expected++);
    auto packed=lunarPacked(index);
    assert(((packed>>5)&15)<12 && (packed&31)<30 && (packed>>10)<60);
    for(int i=2;i<6;++i)assert(ALMANAC_DAYS[index][i]<sizeof(ALMANAC_WORDS)/sizeof(ALMANAC_WORDS[0]));
  }
  assert(expected==1096);
  int today=almanacIndex(2026,11,8);auto p=lunarPacked(today);
  assert(((p>>5)&15)==8 && (p&31)==29 && (p>>10)==42);
  assert(std::strcmp(ALMANAC_WORDS[ALMANAC_DAYS[today][2]],"安床")==0);
  assert(std::strcmp(ALMANAC_WORDS[ALMANAC_DAYS[today][4]],"入宅")==0);
  assert((lunarPacked(almanacIndex(2026,2,16))>>10)==41);
  assert((lunarPacked(almanacIndex(2026,2,17))>>10)==42);
  std::cout<<"PASS: 6-card cycle, ten-minute boundary/rollover, fixed/stock priority, all 1096 lunar records, leap days and lunar year boundary\n";
}
