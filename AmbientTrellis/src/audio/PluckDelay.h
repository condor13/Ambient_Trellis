// PluckDelay - small ping-pong echo for the plucks.
//
// Echoes bounce left, right, left ... and get a little darker and quieter each
// time. It runs at half the sample rate (16 kHz): half the memory, and the
// repeats are soft anyway.
#pragma once
#include <stdint.h>

class PluckDelay {
 public:
  // Longest delay: a dotted 8th at 53 BPM = 0.85 s = 13585 samples at 16 kHz.
  static constexpr int32_t MAX = 14000;

  void init();

  // Settings
  int32_t feedback = 15000;   // Q15, how much of each echo comes back
  int32_t damping = 12000;    // Q15 low-pass in the loop (lower = darker echoes)
  void setTime(uint32_t samples32k);   // delay time in 32 kHz samples

  // Reads `in`, writes only the echoes to `out` (both interleaved stereo int32).
  // `frames` must be even.
  void process(const int32_t *in, int32_t *out, int frames);

 private:
  int16_t bufL_[MAX];
  int16_t bufR_[MAX];
  int32_t pos_ = 0;
  int32_t len_ = MAX - 1;
  int32_t lp_ = 0;
  int32_t lastL_ = 0, lastR_ = 0;
};
