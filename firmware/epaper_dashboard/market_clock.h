#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

enum class Phase { Closed, Auction, Opening, Trading, Closing, Unknown };

inline bool isTradingDate(int year, int month, int day, int weekday,
                          int calendarYear, const char* holidays) {
  if (year != calendarYear || weekday == 0 || weekday == 6) return false;
  char date[9];
  snprintf(date, sizeof(date), "%04d%02d%02d", year, month, day);
  return strstr(holidays, date) == nullptr;
}

inline Phase marketPhase(bool clockValid, bool calendarValid, bool tradingDay, int minutes) {
  if (!clockValid || !calendarValid) return Phase::Unknown;
  if (!tradingDay) return Phase::Closed;
  if (minutes >= 555 && minutes < 565) return Phase::Auction;
  if (minutes >= 565 && minutes < 570) return Phase::Opening;
  if ((minutes >= 570 && minutes < 690) || (minutes >= 780 && minutes < 897)) return Phase::Trading;
  if (minutes >= 897 && minutes < 900) return Phase::Closing;
  return Phase::Closed;
}

// Lunch is compressed; 11:30 and 13:00 share the midpoint.
inline int minuteOffset(int hhmm) {
  const int h = hhmm / 100, m = hhmm % 100;
  if (m < 0 || m >= 60 || h < 0 || h > 23) return -1;
  const int value = h * 60 + m;
  if (value >= 570 && value <= 690) return value - 570;
  if (value >= 780 && value <= 900) return value - 780 + 120;
  return -1;
}

inline float volumeWeightedPrice(double amount, double volumeHands) {
  if (!(amount > 0) || !(volumeHands > 0) || !std::isfinite(amount) || !std::isfinite(volumeHands)) return NAN;
  return amount / (volumeHands * 100.0); // Tencent A-share cumulative volume uses lots of 100 shares.
}

// Avoid pulling the general floating-point scanf engine into the small app
// partition. Preserve the four Tencent fields and their float/double precision.
inline bool parseMinuteRow(const char* row,int& hhmm,float& price,double& volume,double& amount) {
  if(!row)return false;
  char* end=nullptr;long clock=std::strtol(row,&end,10);
  if(end==row || clock<0 || clock>2359)return false;
  hhmm=static_cast<int>(clock);row=end;price=std::strtof(row,&end);if(end==row)return false;
  row=end;volume=std::strtod(row,&end);if(end==row)return false;
  row=end;amount=std::strtod(row,&end);return end!=row;
}

inline bool stockPage(Phase phase) {
  return phase == Phase::Auction || phase == Phase::Opening || phase == Phase::Trading || phase == Phase::Closing;
}
