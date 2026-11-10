#pragma once
#include <cstdint>
inline uint32_t remainingMs(uint32_t now,uint32_t started,uint32_t duration) {
  uint32_t elapsed=now-started;return elapsed>=duration?0:duration-elapsed;
}
inline uint32_t minuteWakeDelayMs(int seconds,int microseconds) {
  return static_cast<uint32_t>((60-seconds%60)*1000-microseconds/1000);
}
inline bool sameClockMinute(int64_t first,int64_t second) { return first/60==second/60; }
