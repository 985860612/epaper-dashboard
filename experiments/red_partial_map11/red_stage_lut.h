#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Experimental hypothesis, not a vendor-approved partial red LUT. The observed
// native waveform is SSD1683-format Mode 1: C,R,W,B, eight 7-byte groups each.
// 0x33 reads 227 main bytes + EOPT/VGH/VSH1/VSH2/VSL/VCOM (6 bytes).
struct RedStageInfo { unsigned frames=0; unsigned nominalMilliseconds=0; };

inline bool makeRedStageCandidate(const uint8_t* native,size_t length,
                                 uint8_t* output,size_t capacity,RedStageInfo& info) {
  info={};
  if(!native || !output || length!=233 || capacity<227 || native==output)return false;
  const uint8_t observedTail[6]={0x22,0x17,0x41,0xa8,0x32,0x50};
  if(std::memcmp(native+227,observedTail,6)!=0)return false;
  if(native[224]!=2 || native[225]!=0 || native[226]!=0)return false; // 50 Hz, normal scan
  constexpr unsigned group=4;
  const uint8_t* common=native+group*7;
  if(common[0]!=3 || common[5]!=1 || common[6]!=1)return false; // original native repetition only
  const uint8_t* red=native+56+group*7;
  const uint8_t* white=native+112+group*7;
  if((red[2]>>6)!=2 || (red[4]>>6)!=3 || (red[4]&63)<16 ||
     (white[2]>>6)!=2 || (white[3]>>6)!=2 || (white[4]>>6)!=0)return false;
  unsigned frames=0;
  for(unsigned phase=1;phase<=4;++phase) {
    if((common[phase]>>6)!=0)return false; // native DC VCOM, not AC common pulses
    frames+=common[phase]&63;
  }
  if(frames<16 || frames>80 || frames*common[0]>180)return false; // <=3.6 s total at 50 Hz
  for(unsigned table=0;table<4;++table) {
    const uint8_t* src=native+table*56+group*7;
    if(src[0]!=3 || src[5]!=1 || src[6]!=1)return false;
    for(unsigned phase=1;phase<=4;++phase)
      if((src[phase]&63)!=(common[phase]&63))return false;
  }
  std::memset(output,0,227); // all seven other groups disabled
  for(unsigned table=0;table<4;++table) {
    uint8_t* dst=output+table*56;
    std::memcpy(dst,native+table*56+group*7,7);
    // Keep native RP=3 and SR=1/1; no pulse amplitude/length changes.
    // Hold class has zero SOURCE level; native VCOM=-2 V remains. This is NOT
    // zero differential voltage and may tint the background. Requires observation.
    if(table==3)for(unsigned phase=1;phase<=4;++phase)dst[phase]&=0x3f;
  }
  std::memcpy(output+224,native+224,3);
  info.frames=frames*common[0];info.nominalMilliseconds=info.frames*1000/50;
  return true;
}

// Match the proven native GxEPD2 tri-color write path: red R/BW=11.
inline uint8_t probeMaskByte(uint8_t command,int byteX,int y) {
  const bool erase=y>=136 && y<176 && byteX>=9 && byteX<14;
  const bool add=y>=136 && y<176 && byteX>=20 && byteX<25;
  return command==0x24 ? ((erase || add)?0xff:0x00) : (add?0xff:0x00);
}
