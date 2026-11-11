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
  assert(BatteryReading{3794}.centivolts()==429);
  assert(BatteryReading{3795}.centivolts()==430);
  assert(BatteryReading{3500}.displayMillivolts()==4000);
  assert(BatteryReading{4200}.displayMillivolts()==4700);
  // The compensated display can exceed 4.5V without changing the power source.
  assert(!BatteryReading{4200}.charging());
  assert(BatteryReading{4500}.displayMillivolts()==5000 && !BatteryReading{4500}.charging());
  assert(BatteryReading{4502}.displayMillivolts()==4502 && BatteryReading{4502}.charging());
  assert(BatteryReading{5000}.displayMillivolts()==5000 && BatteryReading{5000}.charging());
  assert(BatteryReading{6000}.displayMillivolts()==6000);
  // Equal displayed voltages still need distinct keys for the charging icon.
  assert(BatteryReading{4500}.centivolts()==BatteryReading{5000}.centivolts());
  assert(BatteryReading{4500}.displayKey()!=BatteryReading{5000}.displayKey());
  assert(BatteryReading{4500}.displayKey()!=BatteryReading{4502}.displayKey());
  assert(!BatteryReading{0}.charging() && BatteryReading{0}.displayKey()==0);
  assert(BatteryReading{0}.displayMillivolts()==0 && BatteryReading{1998}.displayMillivolts()==1998);
  assert(BatteryReading{6002}.displayMillivolts()==6002);
  std::cout<<"PASS: divider, battery-only +0.50V correction, raw >4.5V detection, rounding, invalid ADC and icon transitions\n";
}
