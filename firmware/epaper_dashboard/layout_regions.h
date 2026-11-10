#pragma once
#include "card_schedule.h"
struct RefreshRegion {int x,y,w,h;const char* name;};
inline int layoutRegions(RefreshRegion* r,bool proof,bool stocks,bool valid,CardId card) {
  if(proof) {r[0]={280,0,120,48,"font_time"};return 1;}
  if(stocks) {r[0]={16,0,384,44,"header"};r[1]={0,44,400,256,"stock"};return 2;}
  if(!valid) {r[0]={16,0,384,52,"header"};r[1]={40,80,320,120,"clock"};r[2]={0,224,400,76,"weather"};return 3;}
  switch(card) {
    case CardId::BigCalendar:r[0]={280,0,120,52,"time"};r[1]={0,244,400,56,"weather"};return 2;
    case CardId::MonthWeather:r[0]={280,0,120,52,"time"};r[1]={16,192,104,48,"time"};r[2]={0,244,400,56,"weather"};return 3;
    case CardId::Almanac:r[0]={288,268,112,32,"time"};return 1;
    case CardId::MinimalClock:r[0]={40,80,320,120,"time"};r[1]={0,224,400,76,"weather"};return 2;
    case CardId::Overview:r[0]={160,64,240,96,"time"};r[1]={0,176,400,48,"weather"};r[2]={0,232,400,68,"quote"};return 3;
  }return 0;
}
