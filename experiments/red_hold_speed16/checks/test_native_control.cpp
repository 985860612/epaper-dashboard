#include "../native_control.h"
#include <array>
#include <cassert>
#include <cstdio>

int main() {
  // Actual HINK 233-byte native readback archived on 2026-11-08, not an
  // unrelated panel's LUT or a synthetic waveform invented for this test.
  const char* hex=
    "010e3d1d0c02020103031414080101030314140801010303141408010305040f220101040506022201010305040122010101040000000101"
    "010e3d9d8c02020143835494080101438354940801014383549408010305840fe2010104050682e2010103058401e2010101040000000101"
    "010e7d5d0c02020143835494080101438394940801014383949408010305848f220101040586822201010305840122010101040000000101"
    "018e3d5d0c02020143835494080101438394940801014383949408010345040f220101044506022201010345040122010101040000000101"
    "020000221741a83250";
  assert(std::strlen(hex)==466);
  std::array<uint8_t,233> native{};
  for(unsigned i=0;i<native.size();++i) { unsigned value=0;assert(std::sscanf(hex+2*i,"%2x",&value)==1);native[i]=value; }
  const auto original=native;
  std::array<uint8_t,227> output{};NativeControlInfo info;
  assert(makeNativeHold(native.data(),233,output.data(),227,info));
  assert(native==original);
  for(unsigned table=0;table<3;++table)for(unsigned i=0;i<56;++i)
    assert(output[table*56+i]==((i==14 || i==21)?0:native[table*56+i]));
  for(unsigned table=0;table<4;++table)for(unsigned group=4;group<=6;++group)
    for(unsigned i=0;i<7;++i)
      if(table!=3 || i==0 || i>=5)assert(output[table*56+group*7+i]==native[table*56+group*7+i]);
  for(unsigned i=168;i<224;++i) {
    unsigned position=(i-168)%7;
    if(position>=1 && position<=4)assert(output[i]==(native[i]&63));
    else assert(output[i]==(((i-168)==14 || (i-168)==21)?0:native[i]));
  }
  assert(std::memcmp(native.data()+224,output.data()+224,3)==0);
  unsigned eraseBytes=0, addBytes=0, outsideNonzero=0;
  for(int y=0;y<300;++y)for(int bx=0;bx<50;++bx) {
    const bool erase=y>=136 && y<176 && bx>=9 && bx<14;
    const bool add=y>=136 && y<176 && bx>=20 && bx<25;
    auto bw=heldMaskByte(0x24,bx,y), red=heldMaskByte(0x26,bx,y);
    if(erase) { ++eraseBytes;assert(bw==255 && red==0); }
    else if(add) { ++addBytes;assert(bw==255 && red==255); }
    else if(bw || red)++outsideNonzero;
  }
  assert(eraseBytes==200 && addBytes==200 && outsideNonzero==0);
  assert(info.frames==818 && info.nominalMilliseconds==16360);
  assert(!makeNativeHold(native.data(),70,output.data(),227,info));
  auto changed=native;changed[232]^=1;
  assert(!makeNativeHold(changed.data(),233,output.data(),227,info));
  changed=native;changed[224]=1;
  assert(!makeNativeHold(changed.data(),233,output.data(),227,info));
  changed=native;changed[0]=10;
  assert(!makeNativeHold(changed.data(),233,output.data(),227,info));
  assert(nativeWireByte(0x24,0xff,0xff)==0xff && nativeWireByte(0x26,0xff,0xff)==0); // white
  assert(nativeWireByte(0x24,0xff,0)==0xff && nativeWireByte(0x26,0xff,0)==0xff); // red11
  assert(nativeWireByte(0x24,0,0xff)==0 && nativeWireByte(0x26,0,0xff)==0); // black00
  std::puts("PASS: 818 frames; only prep2/3 repeats disabled; red-forming groups preserved; hold-source and target-mask bounds, conservative rejection and normal red/white/black RAM polarity");
}
