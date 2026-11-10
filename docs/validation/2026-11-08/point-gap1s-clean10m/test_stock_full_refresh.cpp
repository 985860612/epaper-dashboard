#include <cassert>
#include <cstdint>
#include <iostream>
constexpr uint32_t STOCK_FULL_REFRESH_INTERVAL_MS = 10UL*60UL*1000UL;
constexpr bool stockFullRefreshDue(uint32_t nowMs,uint32_t lastFullMs) {
  return static_cast<uint32_t>(nowMs-lastFullMs)>=STOCK_FULL_REFRESH_INTERVAL_MS;
}
int main() {
 static_assert(STOCK_FULL_REFRESH_INTERVAL_MS==600000UL);
 for(uint32_t last: {0u,17u,1000000u,0xfffffff0u}) {
  assert(!stockFullRefreshDue(last,last));
  assert(!stockFullRefreshDue(static_cast<uint32_t>(last+599999u),last));
  assert(stockFullRefreshDue(static_cast<uint32_t>(last+600000u),last));
  assert(stockFullRefreshDue(static_cast<uint32_t>(last+600001u),last));
  uint32_t finished=static_cast<uint32_t>(last+620000u);
  assert(!stockFullRefreshDue(finished,finished));
  assert(!stockFullRefreshDue(static_cast<uint32_t>(finished+599999u),finished));
  assert(stockFullRefreshDue(static_cast<uint32_t>(finished+600000u),finished));
 }
 std::cout<<"PASS: ten-minute boundary, no early refresh, timer restarts on completion, millis rollover\n";
}
