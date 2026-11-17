// PadMap - turning the pad grid.
//
// All the code works with "logical" pads 0..15, row by row from the top left
// (pad 0 = PLAY in the menu). PAD_ROTATION in Config.h turns that grid on the
// real board. Only the NeoTrellis driver (and the display's pad positions)
// need to know: everything else just uses logical pads.
#pragma once
#include <stdint.h>
#include "../Config.h"

namespace padmap {

// Where a logical pad (row r, column c) sits on the board after turning the
// grid clockwise, as a physical pad number 0..15.
inline uint8_t toPhysical(uint8_t logical) {
  int r = logical / 4, c = logical % 4, pr, pc;
  switch (cfg::PAD_ROTATION) {
    case 90:  pr = c;     pc = 3 - r; break;   // top row becomes the right column
    case 180: pr = 3 - r; pc = 3 - c; break;
    case 270: pr = 3 - c; pc = r;     break;
    default:  pr = r;     pc = c;     break;
  }
  return (uint8_t)(pr * 4 + pc);
}

// The opposite: which logical pad is at this physical place.
inline uint8_t toLogical(uint8_t physical) {
  for (uint8_t l = 0; l < 16; l++)
    if (toPhysical(l) == physical) return l;
  return physical;
}

}  // namespace padmap
