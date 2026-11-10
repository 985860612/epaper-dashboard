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
  assert(copyNativeControl(native.data(),233,output.data(),227,info));
  assert(native==original && std::memcmp(native.data(),output.data(),227)==0);
  assert(info.frames==994 && info.nominalMilliseconds==19880);
  assert(!copyNativeControl(native.data(),70,output.data(),227,info));
  auto changed=native;changed[232]^=1;
  assert(!copyNativeControl(changed.data(),233,output.data(),227,info));
  changed=native;changed[224]=1;
  assert(!copyNativeControl(changed.data(),233,output.data(),227,info));
  changed=native;changed[0]=10;
  assert(!copyNativeControl(changed.data(),233,output.data(),227,info));
  assert(nativeWireByte(0x24,0xff,0xff)==0xff && nativeWireByte(0x26,0xff,0xff)==0); // white
  assert(nativeWireByte(0x24,0xff,0)==0xff && nativeWireByte(0x26,0xff,0)==0xff); // red11
  assert(nativeWireByte(0x24,0,0xff)==0 && nativeWireByte(0x26,0,0xff)==0); // black00
  std::puts("PASS: native 227-byte waveform unmodified, 994 frames, conservative rejection and normal red/white/black RAM polarity");
}
