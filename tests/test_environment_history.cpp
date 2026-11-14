#include "../firmware/epaper_dashboard/environment_history.h"
#include <array>
#include <cassert>
#include <iostream>

int main() {
  constexpr uint32_t start=1794614400; // 2026-11-14 00:00 UTC; a ten-minute boundary.
  static_assert(start%600==0);
  EnvironmentReading valid{true,true,23.4f,56.7f,1013.f};
  EnvironmentHistory h;
  assert(!h.record(0,valid) && !h.record(-1,valid) && !h.record(int64_t(UINT32_MAX)+1,valid));
  assert(h.record(start+23,valid));
  assert(!h.record(start+599,valid));
  assert(h.at(start/600)->epoch==start+23 && h.at(start/600)->temperature==234 && h.at(start/600)->humidity==567);
  assert(h.count(start+10)==0 && h.latest(start+10)==nullptr);
  assert(h.record(start+10,valid) && h.latest(start+10)->epoch==start+10);
  assert(h.record(start+600,valid) && h.count(start+600)==2);
  // A fixed ring retains exactly the last 144 buckets across many revolutions.
  for(uint32_t i=2;i<1440;++i) {
    assert(h.record(start+i*600,valid));
    assert(h.count(start+i*600)==int(i<143?i+1:144));
    if(i>=144)assert(!h.at(start/600+i-144));
  }
  auto epoch=start+1439*600;
  assert(h.latest(epoch)->epoch==epoch);
  std::array<uint8_t,ENV_HISTORY_BYTES> bytes{};encodeHistory(h,bytes.data());
  assert(bytes.size()==1168 && historyU32(bytes.data()+4)==144 && historyU32(bytes.data()+8)==600);
  EnvironmentHistory rebooted;assert(decodeHistory(bytes.data(),bytes.size(),rebooted));
  assert(rebooted.count(epoch)==144 && !rebooted.record(epoch+599,valid));
  assert(rebooted.record(epoch+600,valid));
  assert(!rebooted.at((epoch+600)/600-144));
  std::array<uint8_t,ENV_HISTORY_BYTES> before{};encodeHistory(rebooted,before.data());
  // Torn/corrupt/old-schema storage must not replace the last good in-memory set.
  for(size_t i=0;i<bytes.size();++i) {
    auto broken=bytes;broken[i]^=0x20;
    assert(!decodeHistory(broken.data(),broken.size(),rebooted));
  }
  assert(!decodeHistory(bytes.data(),bytes.size()-1,rebooted));
  std::array<uint8_t,ENV_HISTORY_BYTES> after{};encodeHistory(rebooted,after.data());assert(before==after);
  auto invalid=bytes;invalid[20]=0xff;invalid[21]=0x7f;historyPut32(invalid.data()+12,historyCrc(invalid.data()));
  assert(!decodeHistory(invalid.data(),invalid.size(),rebooted));
  invalid=bytes;historyPut32(invalid.data()+16,1);historyPut32(invalid.data()+12,historyCrc(invalid.data()));
  assert(!decodeHistory(invalid.data(),invalid.size(),rebooted));
  // Reboot/outage gaps are absent, rather than repeated/interpolated measurements.
  EnvironmentHistory gap;
  assert(gap.record(start,valid) && gap.record(start+1800,EnvironmentReading{}));
  assert(!gap.at(start/600+1) && !gap.at(start/600+2));
  assert(!gap.latest(start+1800)->hasTemperature() && !gap.latest(start+1800)->hasHumidity());
  assert(!gap.record(start+1801,valid));
  assert(gap.record(start+2400,valid) && gap.count(start+2400)==3);
  assert(gap.count(start+86400)==2);
  assert(gap.record(start+2*86400,valid) && gap.count(start+2*86400)==1);
  // On backward NTP correction, discard future points and accept the real slot.
  assert(gap.record(start+600,valid) && gap.count(start+600)==1);
  assert(!gap.at((start+2*86400)/600));
  EnvironmentReading extreme{true,true,-40.f,100.f,300.f};
  assert(gap.record(start+1200,extreme));
  assert(gap.latest(start+1200)->temperature==-400 && gap.latest(start+1200)->humidity==1000);
  extreme.temperature=85;extreme.humidity=0;assert(gap.record(start+1800,extreme));
  assert(gap.latest(start+1800)->temperature==850 && gap.latest(start+1800)->humidity==0);
  extreme.temperature=NAN;extreme.humidity=101;assert(gap.record(start+2400,extreme));
  assert(!gap.latest(start+2400)->hasTemperature() && !gap.latest(start+2400)->hasHumidity());
  assert(gap.record(UINT32_MAX,valid) && gap.count(UINT32_MAX)==1);
  std::cout<<"PASS: 10-minute slots, 144-point rolling storage, reboot deduplication, gaps, invalid readings, clock correction, fixed encoding and corruption rejection\n";
}
