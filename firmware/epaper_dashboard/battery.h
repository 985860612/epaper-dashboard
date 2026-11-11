#pragma once
#include <stdint.h>

// Nologo SuperMini: VBUS -- 100k -- GPIO3 -- 100k -- GND.
// On battery power VBUS is after PD1. The user reports a 0.50V shortfall
// on this board; apply that estimate to the display, never to USB detection.
constexpr uint16_t BATTERY_DIODE_DROP_MV=500;
struct BatteryReading {
  uint16_t busMillivolts=0;
  bool valid() const { return busMillivolts>=2000 && busMillivolts<=6000; }
  bool charging() const { return valid() && busMillivolts>4500; }
  uint16_t displayMillivolts() const {
    return busMillivolts+(valid() && !charging()?BATTERY_DIODE_DROP_MV:0);
  }
  uint16_t centivolts() const { return (displayMillivolts()+5)/10; }
  uint8_t bars() const {
    if(!valid() || charging())return 0;
    const uint16_t mv=displayMillivolts();
    return 1+(mv>=3600)+(mv>=3800)+(mv>=4000);
  }
  // Include bars separately: a threshold can change before rounded volts do.
  uint16_t displayKey() const { return valid()?centivolts()*16+(charging()?8:bars()):0; }
};
inline BatteryReading batteryFromAdc(uint32_t pinMillivolts) {
  return {static_cast<uint16_t>(pinMillivolts<=3000?pinMillivolts*2:0)};
}
extern BatteryReading battery;
void serviceBattery();
