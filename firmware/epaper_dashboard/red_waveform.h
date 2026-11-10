#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
struct NativeControlInfo { unsigned frames=0;unsigned nominalMilliseconds=0; };
inline bool makeNativeHold(const uint8_t* native,size_t length,uint8_t* output,size_t capacity,NativeControlInfo& info) {
  info={};if(!native || !output || length!=233 || capacity<227 || native==output)return false;
  // Only the two complete native profiles actually captured from this panel.
  // This integrity check rejects any different input instead of guessing volts.
  uint32_t fingerprint=2166136261u;
  for(unsigned i=0;i<233;++i)fingerprint=(fingerprint^native[i])*16777619u;
  const bool profileA8=fingerprint==0x72fd04adu;
  const bool profileA4=fingerprint==0xe08a1751u;
  if(!profileA8 && !profileA4)return false;
  const uint8_t tail[]={0x22,0x17,0x41,static_cast<uint8_t>(profileA8?0xa8:0xa4),0x32,0x50};
  if(std::memcmp(native+227,tail,6) || native[224]!=2 || native[225]!=0 || native[226]!=0)return false;
  unsigned total=0;
  for(unsigned group=0;group<8;++group) {
    const uint8_t* common=native+group*7;
    for(unsigned table=1;table<4;++table) {
      const uint8_t* row=native+table*56+group*7;
      if(row[0]!=common[0] || row[5]!=common[5] || row[6]!=common[6])return false;
      for(unsigned p=1;p<=4;++p)if((row[p]&63)!=(common[p]&63))return false;
    }
    for(unsigned p=1;p<=4;++p) {
      if((common[p]>>6)!=0)return false;
      total+=common[0]*(common[p]&63)*common[p<=2?5:6];
    }
  }
  if(total!=(profileA8?994u:1350u))return false;
  std::memcpy(output,native,227);
  // Keep complete VCOM/RED/WHITE waveforms and all native timing. Only the
  // source levels of the unchanged-pixel LUTB become VSS. VCOM stays native,
  // so this is NOT guaranteed zero differential voltage; observe background.
  for(unsigned g=0;g<8;++g)for(unsigned p=1;p<=4;++p)output[168+g*7+p]&=0x3f;
  // Both captured profiles have their full red-forming sequence in 4/5/6.
  // Keep group0 and 4/5/6/7 intact, skipping prep1/2/3 for every LUT.
  for(unsigned table=0;table<4;++table) {
    output[table*56+7]=0;output[table*56+14]=0;output[table*56+21]=0;
  }
  // Restore a bounded native WHITE-only cleaning group. All four LUTs keep
  // synchronized native TP values; RED and hold-source levels stay at VSS.
  // Repeats are never greater than those in the current native profile.
  for(unsigned table=0;table<4;++table) {
    uint8_t* row=output+table*56+7;const uint8_t* src=native+table*56+7;
    std::memcpy(row,src,7);row[5]=src[5]<3?src[5]:3;row[6]=src[6]<8?src[6]:8;
    if(table==1 || table==3)for(unsigned phase=1;phase<=4;++phase)row[phase]&=0x3f;
  }
  unsigned shortened=0;
  for(unsigned group=0;group<8;++group) {
    const uint8_t* common=output+group*7;
    for(unsigned p=1;p<=4;++p)shortened+=common[0]*(common[p]&63)*common[p<=2?5:6];
  }
  if(shortened!=(profileA8?788u:721u) || shortened>=total)return false;
  info.frames=shortened;info.nominalMilliseconds=shortened*1000/50;return true;
}
