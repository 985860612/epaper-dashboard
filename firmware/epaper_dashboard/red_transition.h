#pragma once
#include <cstdint>
// Logical frame planes: 1=absent, 0=ink. Mode 1 RAM codes (R/BW):
// 00=hold, 01=erase to white, 11=form red. Never use 10 here.
struct RedTransition {
  uint8_t bw, red;
  uint8_t blackAfterRed(uint8_t before) const { return before | bw; }
};
inline RedTransition redTransition(uint8_t before, uint8_t after) {
  return {static_cast<uint8_t>(before ^ after), static_cast<uint8_t>(before & ~after)};
}
