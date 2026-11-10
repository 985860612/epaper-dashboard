#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <cstdio>
#include "/Users/wangxiaojie/xj-studio/.worktrees/epaper-stock-replay/projects/epaper-dashboard/firmware/epaper_dashboard/market_clock.h"
constexpr int STOCK_REPLAY_STEPS = 12;
constexpr int stockReplayVisibleCount(int total,int step) {
  return total<=0 ? 0 : step<=0 ? 1 : step>=STOCK_REPLAY_STEPS ? total
      : 1+((total-1)*step+STOCK_REPLAY_STEPS-1)/STOCK_REPLAY_STEPS;
}
int main(int argc,char** argv) {
 assert(argc==2);
 assert(stockReplayVisibleCount(0,0)==0);
 for(int total=1;total<=245;++total) {
  int old=stockReplayVisibleCount(total,0);assert(old==1);
  for(int step=1;step<=STOCK_REPLAY_STEPS;++step) {
   int next=stockReplayVisibleCount(total,step);
   assert(next>=old && next>=1 && next<=total);
   if(total>STOCK_REPLAY_STEPS)assert(next>old);
   old=next;
  }
  assert(old==total && stockReplayVisibleCount(total,100)==total);
 }
 std::ifstream input(argv[1]);std::string row;int count=0,lastOffset=-1;float first=0,last=0,lastAvg=0;
 while(std::getline(input,row) && count<245) {
  int hhmm;float price;double volume,amount;
  if(sscanf(row.c_str(),"%d %f %lf %lf",&hhmm,&price,&volume,&amount)!=4)continue;
  int offset=minuteOffset(hhmm);if(offset<0 || offset<lastOffset || !(price>0) || !std::isfinite(price))continue;
  if(count==0)first=price;
  last=price;lastOffset=offset;lastAvg=volumeWeightedPrice(amount,volume);++count;
 }
 assert(count>12 && count<=245 && lastOffset==240);
 assert(first>0 && last>0 && std::isfinite(lastAvg));
 for(int step=0;step<=STOCK_REPLAY_STEPS;++step)std::cout<<"STEP "<<step<<" visible="<<stockReplayVisibleCount(count,step)<<"/"<<count<<"\n";
 std::cout<<"PASS: all replay sizes monotonic, full final coverage, valid source session; points="<<count<<" first="<<first<<" last="<<last<<" lastAvg="<<lastAvg<<"\n";
}
