#include "../firmware/epaper_dashboard/power_schedule.h"
#include <cassert>
#include <iostream>
int main() {
  assert(minuteWakeDelayMs(0,0)==60000);
  assert(minuteWakeDelayMs(59,999999)==1);
  assert(minuteWakeDelayMs(30,500000)==29500);
  for(uint32_t start:{0u,1234u,0xffffff00u}) {
    assert(remainingMs(start,start,600000)==600000);
    assert(remainingMs(start+599999,start,600000)==1);
    assert(remainingMs(start+600000,start,600000)==0);
    assert(remainingMs(start+600001,start,600000)==0);
  }
  // Long refreshes crossing a minute must schedule another sample/header
  // before sleeping, rather than skip an entire minute of data or display.
  assert(sameClockMinute(3599,3540));
  assert(!sameClockMinute(3600,3599));
  assert(!sameClockMinute(3610,3590));
  assert(sameClockMinute(3620,3610));
  std::cout<<"PASS: minute/microsecond boundaries, awake-window expiry, millis rollover, cross-minute rendering\n";
}
