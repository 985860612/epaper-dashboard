#include "../firmware/epaper_dashboard/market_clock.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  assert(stockPage(Phase::Auction) && stockPage(Phase::Opening));
  assert(stockPage(Phase::Trading) && stockPage(Phase::Closing));
  assert(!stockPage(Phase::Closed) && !stockPage(Phase::Unknown));
  assert(marketPhase(true,true,true,554)==Phase::Closed);
  assert(marketPhase(true,true,true,555)==Phase::Auction);
  assert(marketPhase(true,true,true,564)==Phase::Auction);
  assert(marketPhase(true,true,true,565)==Phase::Opening);
  assert(marketPhase(true,true,true,570)==Phase::Trading);
  assert(marketPhase(true,true,true,689)==Phase::Trading);
  assert(marketPhase(true,true,true,690)==Phase::Closed);
  assert(marketPhase(true,true,true,779)==Phase::Closed);
  assert(marketPhase(true,true,true,780)==Phase::Trading);
  assert(marketPhase(true,true,true,897)==Phase::Closing);
  assert(marketPhase(true,true,true,900)==Phase::Closed);
  assert(marketPhase(true,true,false,600)==Phase::Closed);
  assert(marketPhase(false,true,true,600)==Phase::Unknown);
  assert(marketPhase(true,false,true,600)==Phase::Unknown);
  assert(!isTradingDate(2026,10,1,4,2026,"20261001,20261002"));
  assert(!isTradingDate(2026,10,10,6,2026,"")); // Government makeup Saturday is not a trading day.
  assert(isTradingDate(2026,11,9,1,2026,"20261001,20261002"));
  assert(!isTradingDate(2027,11,9,2,2026,""));
  // Auto view stays on clock throughout a Sunday, including trading hours.
  for(int minute=0;minute<1440;++minute) {
    assert(!stockPage(marketPhase(true,true,false,minute)));
  }
  assert(minuteOffset(915)==-1);
  assert(minuteOffset(930)==0);
  assert(minuteOffset(1130)==120);
  assert(minuteOffset(1200)==-1);
  assert(minuteOffset(1300)==120);
  assert(minuteOffset(1500)==240);
  assert(minuteOffset(1501)==-1);
  assert(minuteOffset(960)==-1);
  assert(fabs(volumeWeightedPrice(106672575.0,36345.0)-29.35)<0.0001);
  assert(std::isnan(volumeWeightedPrice(100,0)));
  assert(std::isnan(volumeWeightedPrice(NAN,100)));
  for(const char* row:{"0930 29.35 36345 106672575", " 1500\t31.25 1e6 3.125e9 extra", "1130 0 0 0", "1300 -1 -2 -3"}) {
    int hhmm=0,expectedClock=0;float price=0,expectedPrice=0;double volume=0,amount=0,expectedVolume=0,expectedAmount=0;
    assert(std::sscanf(row,"%d %f %lf %lf",&expectedClock,&expectedPrice,&expectedVolume,&expectedAmount)==4);
    assert(parseMinuteRow(row,hhmm,price,volume,amount));
    assert(hhmm==expectedClock && price==expectedPrice && volume==expectedVolume && amount==expectedAmount);
  }
  for(const char* row:{static_cast<const char*>(nullptr),"","0930","0930 29.35","0930 29.35 100","oops 29.35 100 100","0930 nope 100 100","0930 29.35 nope 100","0930 29.35 100 nope","99999999999999999999 1 2 3"}) {
    int hhmm=0;float price=0;double volume=0,amount=0;assert(!parseMinuteRow(row,hhmm,price,volume,amount));
  }
  std::cout << "PASS: session boundaries, lunch compression, holidays/year expiry, and volume-weighted price\n";
}
