#include "../firmware/epaper_dashboard/red_transition.h"
#include "../firmware/epaper_dashboard/red_waveform.h"
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>

enum Ink { White, Black, Red };
static void checkPixels() {
  // Exhaust every source/target color pair at every bit position. Model the
  // two display passes separately to catch erasing newly formed red, leaving
  // a black shadow, or failing to draw black where red was erased.
  for(auto from:{White,Black,Red})for(auto to:{White,Black,Red})for(int bit=0;bit<8;++bit) {
    const uint8_t mask=0x80>>bit;
    uint8_t oldRed=from==Red?uint8_t(255^mask):255;
    uint8_t newRed=to==Red?uint8_t(255^mask):255;
    uint8_t oldBlack=from==Black?uint8_t(255^mask):255;
    uint8_t newBlack=to==Black?uint8_t(255^mask):255;
    auto delta=redTransition(oldRed,newRed);
    assert(!(delta.red & ~delta.bw)); // forbidden code 10 never emitted
    Ink physical=from;
    if(delta.bw & mask)physical=(delta.red & mask)?Red:White;
    uint8_t after=delta.blackAfterRed(oldBlack);
    if((after ^ newBlack)&mask)physical=(newBlack & mask)?White:Black;
    assert(physical==to);
    // Neighbors stay held, including other static red pixels.
    assert((delta.bw & ~mask)==0 && (delta.red & ~mask)==0);
  }
  auto mixed=redTransition(0x0f,0x33);
  assert(mixed.bw==0x3c && mixed.red==0x0c);
  assert(redTransition(0x55,0x55).bw==0);
}
static void checkProfile(const char* file,unsigned expectedFrames) {
  std::ifstream input(file,std::ios::binary);
  std::array<uint8_t,233> native{};input.read(reinterpret_cast<char*>(native.data()),native.size());
  assert(input.gcount()==233);
  const auto original=native;
  std::array<uint8_t,227> lut{};NativeControlInfo info;
  assert(makeNativeHold(native.data(),233,lut.data(),227,info));
  assert(native==original && info.frames==expectedFrames);
  assert(info.nominalMilliseconds==expectedFrames*20);
  assert(!makeNativeHold(native.data(),232,lut.data(),227,info));
  assert(!makeNativeHold(native.data(),233,lut.data(),226,info));
  // Corruption anywhere, including voltage/end parameters, fails closed.
  for(size_t i=0;i<native.size();++i) {
    native=original;native[i]^=1;
    assert(!makeNativeHold(native.data(),233,lut.data(),227,info));
  }
}
int main(int argc,char** argv) {
  assert(argc==3);
  checkPixels();checkProfile(argv[1],788);checkProfile(argv[2],721);
  std::cout<<"PASS: all color transitions, static neighbors, mixed masks, two native profiles, 466 corrupted profiles rejected\n";
}
