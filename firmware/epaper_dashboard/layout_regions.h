#pragma once
#include "card_schedule.h"
struct RefreshRegion {int x,y,w,h;const char* name;};
constexpr int MAX_REFRESH_REGIONS=5;
inline int pageRegions(RefreshRegion* r,bool proof,bool stocks,bool valid,CardId card) {
  if(proof) {r[0]={280,0,120,48,"font_time"};return 1;}
  if(stocks) {r[0]={16,0,384,56,"header"};r[1]={0,56,400,244,"stock"};return 2;}
  if(!valid) {r[0]={16,0,384,52,"header"};r[1]={40,56,320,108,"clock"};r[2]={16,176,184,124,"indoor"};r[3]={216,176,168,124,"weather"};return 4;}
  switch(card) {
    case CardId::BigCalendar:r[0]={280,0,120,52,"time"};r[1]={0,244,400,56,"weather"};return 2;
    case CardId::MonthWeather:r[0]={280,0,120,52,"time"};r[1]={16,192,104,48,"time"};r[2]={0,244,400,56,"weather"};return 3;
    case CardId::Almanac:r[0]={200,0,200,52,"time"};r[1]={0,244,400,56,"weather"};return 2;
    case CardId::MinimalClock:r[0]={40,56,320,108,"time"};r[1]={16,176,184,124,"indoor"};r[2]={216,176,168,124,"weather"};return 3;
    case CardId::Overview:r[0]={160,64,240,96,"time"};r[1]={0,176,400,48,"weather"};r[2]={0,232,400,68,"quote"};return 3;
  }return 0;
}
inline int layoutRegions(RefreshRegion* r,bool proof,bool stocks,bool valid,CardId card) {
  int count=pageRegions(r,proof,stocks,valid,card);
  // Keep the status strip separate from red titles in the main header.
  // Normal page content starts below y=12 (covered by the layout tests).
  for(int i=0;i<count;++i)if(r[i].y==0 && r[i].h>=12) {r[i].y=12;r[i].h-=12;}
  r[count++]={320,0,64,12,"power"};return count;
}
