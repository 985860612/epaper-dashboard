#include "../firmware/epaper_dashboard/battery.h"
#include <cassert>
#include <iostream>
int main() {
  assert(batteryFromAdc(1900).busMillivolts==3800);
  assert(!batteryFromAdc(2250).charging());
  assert(batteryFromAdc(2251).charging());
  assert(batteryFromAdc(2500).charging());
  assert(batteryFromAdc(3000).valid());
  for(uint32_t mv:{0u,999u,3001u,UINT32_MAX})assert(!batteryFromAdc(mv).valid());
  assert(BatteryReading{3794}.centivolts()==379);
  assert(BatteryReading{3795}.centivolts()==380);
  // Crossing 4.5V must repaint the icon even if the displayed voltage rounds alike.
  assert(BatteryReading{4500}.displayKey()!=BatteryReading{4502}.displayKey());
  assert(!BatteryReading{0}.charging() && BatteryReading{0}.displayKey()==0);
  std::cout<<"PASS: 2:1 divider, strict >4.5V threshold, rounding, invalid ADC and icon transition\n";
}
