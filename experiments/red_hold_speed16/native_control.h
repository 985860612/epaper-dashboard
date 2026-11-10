#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
struct NativeControlInfo { unsigned frames=0;unsigned nominalMilliseconds=0; };
inline bool makeNativeHold(const uint8_t* native,size_t length,uint8_t* output,size_t capacity,NativeControlInfo& info) {
  info={};if(!native || !output || length!=233 || capacity<227 || native==output)return false;
  const uint8_t tail[]={0x22,0x17,0x41,0xa8,0x32,0x50};
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
  if(total<900 || total>1100)return false; // <=22 s at native 50 Hz, plus power sequence
  std::memcpy(output,native,227);
  // Keep complete VCOM/RED/WHITE waveforms and all native timing. Only the
  // source levels of the unchanged-pixel LUTB become VSS. VCOM stays native,
  // so this is NOT guaranteed zero differential voltage; observe background.
  for(unsigned g=0;g<8;++g)for(unsigned p=1;p<=4;++p)output[168+g*7+p]&=0x3f;
  // Retain the complete red-forming groups 4/5/6 and all native pulse lengths.
  // Reduce the three red preparation groups to group1 only. Groups2 and3 must
  // match each other in this profile, and their RED sequence must match group1.
  for(unsigned table=0;table<4;++table)
    if(std::memcmp(native+table*56+14,native+table*56+21,7)!=0)return false;
  if(std::memcmp(native+56+7,native+56+14,7)!=0)return false;
  for(unsigned table=0;table<4;++table) {
    output[table*56+14]=0;output[table*56+21]=0;
  }
  unsigned shortened=0;
  for(unsigned group=0;group<8;++group) {
    const uint8_t* common=output+group*7;
    for(unsigned p=1;p<=4;++p)shortened+=common[0]*(common[p]&63)*common[p<=2?5:6];
  }
  if(shortened<750 || shortened>900 || shortened>=total)return false;
  info.frames=shortened;info.nominalMilliseconds=shortened*1000/50;return true;
}
inline uint8_t nativeWireByte(uint8_t command,uint8_t bw,uint8_t color) {
  return command==0x24 ? bw : static_cast<uint8_t>(~color);
}

inline uint8_t heldMaskByte(uint8_t command,int byteX,int y) {
  const bool erase=y>=136 && y<176 && byteX>=9 && byteX<14;
  const bool add=y>=136 && y<176 && byteX>=20 && byteX<25;
  return command==0x24 ? ((erase || add)?0xff:0x00) : (add?0xff:0x00);
}
