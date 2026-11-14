#pragma once
#include "environment.h"
#include <cstring>
#include <limits.h>

constexpr uint32_t ENV_HISTORY_INTERVAL=600;
constexpr int ENV_HISTORY_CAPACITY=144;
constexpr uint32_t ENV_HISTORY_MIN_EPOCH=1609459200;
constexpr int16_t ENV_HISTORY_NO_TEMP=INT16_MIN;
constexpr uint16_t ENV_HISTORY_NO_HUMIDITY=UINT16_MAX;

struct EnvironmentHistoryPoint {
  uint32_t epoch=0;
  int16_t temperature=ENV_HISTORY_NO_TEMP; // tenths of a degree Celsius
  uint16_t humidity=ENV_HISTORY_NO_HUMIDITY; // tenths of a percent
  bool hasTemperature() const {return temperature>=-400 && temperature<=850;}
  bool hasHumidity() const {return humidity<=1000;}
};
struct EnvironmentHistory {
  EnvironmentHistoryPoint points[ENV_HISTORY_CAPACITY]{};
  static uint32_t bucket(int64_t epoch) {
    return epoch>=ENV_HISTORY_MIN_EPOCH && epoch<=UINT32_MAX?uint32_t(epoch)/ENV_HISTORY_INTERVAL:0;
  }
  const EnvironmentHistoryPoint* at(uint32_t slot) const {
    const auto& p=points[slot%ENV_HISTORY_CAPACITY];
    return p.epoch && p.epoch/ENV_HISTORY_INTERVAL==slot?&p:nullptr;
  }
  bool record(int64_t epoch,const EnvironmentReading& reading) {
    const uint32_t slot=bucket(epoch);if(!slot)return false;
    bool changed=false;
    // Expired and future-dated points cannot survive a clock correction or a gap.
    for(auto& p:points)if(p.epoch) {
      const uint32_t old=p.epoch/ENV_HISTORY_INTERVAL;
      if(p.epoch>uint32_t(epoch) || old>slot || slot-old>=ENV_HISTORY_CAPACITY) {p={};changed=true;}
    }
    if(at(slot))return changed; // Includes reboot within the same ten-minute slot.
    EnvironmentHistoryPoint p;p.epoch=uint32_t(epoch);
    if(reading.ahtValid && isfinite(reading.temperature) && reading.temperature>=-40 && reading.temperature<=85)
      p.temperature=int16_t(lroundf(reading.temperature*10));
    if(reading.ahtValid && isfinite(reading.humidity) && reading.humidity>=0 && reading.humidity<=100)
      p.humidity=uint16_t(lroundf(reading.humidity*10));
    points[slot%ENV_HISTORY_CAPACITY]=p;return true;
  }
  int count(int64_t epoch) const {
    const uint32_t end=bucket(epoch);if(!end)return 0;
    int n=0;for(uint32_t i=0;i<ENV_HISTORY_CAPACITY;++i)if(const auto* p=at(end-i))if(p->epoch<=epoch)++n;return n;
  }
  const EnvironmentHistoryPoint* latest(int64_t epoch) const {
    const uint32_t end=bucket(epoch);if(!end)return nullptr;
    for(uint32_t i=0;i<ENV_HISTORY_CAPACITY;++i)if(const auto* p=at(end-i))if(p->epoch<=epoch)return p;
    return nullptr;
  }
};

// Explicit little-endian wire format; never persist C++ padding or pointers.
constexpr size_t ENV_HISTORY_BYTES=16+ENV_HISTORY_CAPACITY*8;
inline uint32_t historyU32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline void historyPut32(uint8_t* p,uint32_t v) {for(int i=0;i<4;++i)p[i]=uint8_t(v>>(i*8));}
inline uint32_t historyCrc(const uint8_t* bytes) {
  uint32_t crc=0xffffffffu;
  for(size_t i=0;i<ENV_HISTORY_BYTES;++i)if(i<12 || i>=16) {
    crc^=bytes[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));
  }return ~crc;
}
inline void encodeHistory(const EnvironmentHistory& history,uint8_t* bytes) {
  memcpy(bytes,"ENV1",4);historyPut32(bytes+4,ENV_HISTORY_CAPACITY);historyPut32(bytes+8,ENV_HISTORY_INTERVAL);
  for(int i=0;i<ENV_HISTORY_CAPACITY;++i) {
    const auto& p=history.points[i];auto* out=bytes+16+i*8;
    historyPut32(out,p.epoch);out[4]=uint8_t(p.temperature);out[5]=uint8_t(uint16_t(p.temperature)>>8);
    out[6]=uint8_t(p.humidity);out[7]=uint8_t(p.humidity>>8);
  }historyPut32(bytes+12,historyCrc(bytes));
}
inline bool decodeHistory(const uint8_t* bytes,size_t size,EnvironmentHistory& history) {
  if(size!=ENV_HISTORY_BYTES || memcmp(bytes,"ENV1",4) || historyU32(bytes+4)!=ENV_HISTORY_CAPACITY ||
     historyU32(bytes+8)!=ENV_HISTORY_INTERVAL || historyU32(bytes+12)!=historyCrc(bytes))return false;
  EnvironmentHistory candidate;
  for(int i=0;i<ENV_HISTORY_CAPACITY;++i) {
    auto& p=candidate.points[i];const auto* in=bytes+16+i*8;
    p.epoch=historyU32(in);p.temperature=int16_t(envU16(in+4));p.humidity=envU16(in+6);
    if((p.temperature!=ENV_HISTORY_NO_TEMP && !p.hasTemperature()) ||
       (p.humidity!=ENV_HISTORY_NO_HUMIDITY && !p.hasHumidity()))return false;
    if(p.epoch) {
      if(p.epoch<ENV_HISTORY_MIN_EPOCH || (p.epoch/ENV_HISTORY_INTERVAL)%ENV_HISTORY_CAPACITY!=uint32_t(i))return false;
    } else if(p.temperature!=ENV_HISTORY_NO_TEMP || p.humidity!=ENV_HISTORY_NO_HUMIDITY)return false;
  }history=candidate;return true;
}

extern EnvironmentHistory environmentHistory;
extern const char* environmentHistoryError;
extern uint32_t environmentHistoryRevision;
void initEnvironmentHistory();
void recordEnvironmentHistory(int64_t epoch,const EnvironmentReading& reading);
