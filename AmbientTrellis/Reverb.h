// Reverb - Dattorro-style plate reverb, integer only. Runs on core 1.
//
// Mono in, stereo out. Input diffusers smear the sound, then two cross-coupled
// "tanks" (with slowly moving delays) ring on for a long, dark tail.
// Uses about 50 KB of RAM for its delay lines.
#pragma once
#include <stdint.h>

class Reverb {
 public:
  void init();

  // Settings (Q15: 32768 = 1.0)
  int32_t decay = 27500;     // ~0.84: tail length (higher = longer)
  int32_t damping = 16300;   // tank low-pass amount (higher = brighter)
  int32_t inputBw = 22000;   // input low-pass (higher = brighter)

  // Adds the reverb to `mix` (interleaved stereo int32, 32767 = full scale).
  // wet / dry are Q8 (256 = 1.0).
  void process(int32_t *mix, int frames, int32_t wet, int32_t dry);

 private:
  struct Line {
    int16_t *buf;
    int32_t len, pos;
    // Sample written d writes ago (1 <= d <= len)
    inline int32_t read(int32_t d) const {
      int32_t i = pos - d;
      if (i < 0) i += len;
      return buf[i];
    }
    inline void write(int32_t x) {
      if (x > 32767) x = 32767;
      if (x < -32768) x = -32768;
      buf[pos] = (int16_t)x;
      if (++pos >= len) pos = 0;
    }
  };

  static int32_t allpass(Line &l, int32_t x, int32_t g);
  static int32_t allpassMod(Line &l, int32_t x, int32_t g, int32_t delayQ16);

  Line pre_, ap1_, ap2_, ap3_, ap4_;
  Line lAp_, lD1_, lAp2_, lD2_;     // left tank
  Line rAp_, rD1_, rAp2_, rD2_;     // right tank
  int32_t bw_ = 0, lDamp_ = 0, rDamp_ = 0;
  int32_t lFeed_ = 0, rFeed_ = 0;   // tank cross-feedback
  uint32_t lfo_ = 0;
};
