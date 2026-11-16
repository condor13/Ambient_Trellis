// SoundParams: every sound setting in one place.
//
// Written by core 0 (later: the SHIFT menu), read by core 1 once per block.
// Each field is a single 32-bit word, so reading it is always safe across cores.
#pragma once
#include <stdint.h>

struct SoundParams {
  // Pads
  volatile uint32_t attackMs    = 7000;    // chord swell time
  volatile uint32_t releaseMs   = 7500;    // chord fade time
  volatile uint32_t evolveMs    = 14000;   // how long brightness keeps rising
  volatile uint32_t lfoMs       = 24000;   // period of the slow LFO
  volatile uint32_t brightness  = 22000;   // max FM index (0..52000)

  // Plucks (Q8: 256 = 1.0)
  volatile uint32_t pluckDry    = 70;      // plucks heard directly (soft)
  volatile uint32_t pluckSend   = 256;     // plucks into the reverb (a lot)
  volatile uint32_t pluckDecayMs = 3200;   // how long a pluck rings

  // Pluck delay
  volatile uint32_t delayMix    = 110;     // echoes level, 0 = off (Q8)
  volatile uint32_t delayFeedback = 15000; // Q15, ~0.46
  volatile uint32_t delaySamples = 24000;  // delay time at 32 kHz (set from tempo)

  // Master
  volatile uint32_t volume      = 225;     // 0..256
  volatile uint32_t reverbWet   = 150;     // 0 = dry .. 256 = fully wet
  volatile uint32_t reverbDecay = 27500;   // tail length, Q15 (max ~30000)
};
