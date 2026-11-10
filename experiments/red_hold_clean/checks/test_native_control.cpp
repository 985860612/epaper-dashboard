#include "../native_control.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>

int main(int argc,char** argv) {
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
  for(unsigned table=0;table<4;++table)for(unsigned i=0;i<56;++i) {
    const unsigned group=i/7, pos=i%7;uint8_t expected=native[table*56+i];
    if((group==2 || group==3) && pos==0)expected=0;
    if(group==1 && pos==5)expected=expected<3?expected:3;
    if(group==1 && pos==6)expected=expected<8?expected:8;
    if(pos>=1 && pos<=4 && (table==3 || (table==1 && group==1)))expected&=63;
    assert(output[table*56+i]==expected);
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
  assert(info.frames==788 && info.nominalMilliseconds==15760);
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
  assert(argc==2);
  std::array<uint8_t,233> alternate{};
  std::ifstream in(argv[1],std::ios::binary);assert(in.good());
  in.read(reinterpret_cast<char*>(alternate.data()),alternate.size());assert(in.gcount()==233);
  const auto alternateBefore=alternate;
  assert(makeNativeHold(alternate.data(),233,output.data(),227,info));
  assert(alternate==alternateBefore && info.frames==721 && info.nominalMilliseconds==14420);
  for(unsigned table=0;table<4;++table)for(unsigned i=0;i<56;++i) {
    const unsigned group=i/7,pos=i%7;uint8_t expected=alternate[table*56+i];
    if((group==2 || group==3) && pos==0)expected=0;
    if(group==1 && pos==5)expected=expected<3?expected:3;
    if(group==1 && pos==6)expected=expected<8?expected:8;
    if(pos>=1 && pos<=4 && (table==3 || (table==1 && group==1)))expected&=63;
    assert(output[table*56+i]==expected);
  }
  assert(std::memcmp(output.data()+224,alternate.data()+224,3)==0);
  auto unknown=alternate;unknown[1]^=1;
  assert(!makeNativeHold(unknown.data(),233,output.data(),227,info));
  std::puts("PASS: 788/721 frames; bounded native WHITE cleanup only; RED cleanup phase held, unchanged forming stages; unknown profiles rejected; red-forming groups preserved; hold-source and target-mask bounds, conservative rejection and normal red/white/black RAM polarity");
}
