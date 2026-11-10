#include "../red_stage_lut.h"
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
  std::array<uint8_t,227> candidate{};
  RedStageInfo info;
  assert(makeRedStageCandidate(native.data(),native.size(),candidate.data(),candidate.size(),info));
  assert(native==original);assert(info.frames==174 && info.nominalMilliseconds==3480);
  const uint8_t expected[4][7]={{3,5,4,15,34,1,1},{3,5,0x84,15,0xe2,1,1},
    {3,5,0x84,0x8f,34,1,1},{3,5,4,15,34,1,1}};
  for(unsigned table=0;table<4;++table) {
    assert(std::memcmp(candidate.data()+table*56,expected[table],7)==0);
    for(unsigned i=7;i<56;++i)assert(candidate[table*56+i]==0);
  }
  assert(std::memcmp(candidate.data()+224,native.data()+224,3)==0);
  assert(!makeRedStageCandidate(native.data(),70,candidate.data(),candidate.size(),info));
  assert(!makeRedStageCandidate(native.data(),233,candidate.data(),226,info));
  auto changed=native;changed[232]^=1;
  assert(!makeRedStageCandidate(changed.data(),233,candidate.data(),227,info)); // changed VCOM rejected
  changed=native;changed[28]=4;
  assert(!makeRedStageCandidate(changed.data(),233,candidate.data(),227,info)); // non-native repetition rejected
  changed=native;changed[224]=1;
  assert(!makeRedStageCandidate(changed.data(),233,candidate.data(),227,info)); // slower frame rate rejected
  changed=native;changed[112+28+1]^=1;
  assert(!makeRedStageCandidate(changed.data(),233,candidate.data(),227,info)); // unsynchronised timing rejected
  changed=native;changed[56+28+4]&=63;
  assert(!makeRedStageCandidate(changed.data(),233,candidate.data(),227,info)); // B/W-only/no red stage rejected
  changed=native;
  for(unsigned table=0;table<4;++table)for(unsigned phase=1;phase<=4;++phase)
    changed[table*56+28+phase]=(changed[table*56+28+phase]&0xc0)|63;
  assert(!makeRedStageCandidate(changed.data(),233,candidate.data(),227,info)); // excessive drive budget rejected
  std::puts("PASS: actual native readback, 174-frame native-three-repeat candidate, unchanged native source, disabled groups, voltage/frame/layout/timing/drive-budget rejection");
}
