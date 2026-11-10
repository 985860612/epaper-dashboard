#include <cassert>
#include <iostream>
constexpr int stockReplayVisibleCount(int total,int step) {
  return total<=0 || step<=0 ? 0 : step>=total ? total : step;
}
int main(){
 assert(stockReplayVisibleCount(0,0)==0);
 assert(stockReplayVisibleCount(-1,2)==0);
 for(int total=1;total<=245;++total){
  assert(stockReplayVisibleCount(total,-1)==0);
  int old=stockReplayVisibleCount(total,0);assert(old==0);
  for(int frame=1;frame<=total;++frame){
   int n=stockReplayVisibleCount(total,frame);
   assert(n==old+1 && n==frame && n<=total);old=n;
  }
  assert(old==total && stockReplayVisibleCount(total,total+50)==total);
 }
 std::cout<<"PASS: exactly one new point each frame for 1..245 points; empty initial canvas, no skips or out-of-bounds, full final coverage\n";
}
