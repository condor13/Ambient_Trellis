// PluckDelay - small echo for the plucks, with 4 modes:
//
//   FORWARD       plain echoes in the middle
//   PING_PONG     echoes bounce left, right, left ...
//   REVERSE       each echo plays backwards: it swells in instead of fading
//   REVERSE_PING  backwards on the left, then bouncing forwards to the right
//
// Every repeat is a little darker and quieter. It runs at half the sample
// rate (16 kHz): half the memory, and the repeats are soft anyway.
#pragma once
#include <stdint.h>

class PluckDelay {
 public:
  // Longest delay: a dotted 8th at 53 BPM = 0.85 s = 13585 samples at 16 kHz.
  static constexpr int32_t MAX = 14000;

  void init();

  enum Mode : int32_t { FORWARD = 0, PING_PONG = 1, REVERSE = 2, REVERSE_PING = 3 };

  // Settings
  int32_t mode = PING_PONG;
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
  int32_t revOfs_ = 1;          // reverse: how far back the backwards reader is
  uint32_t revPh_ = 0;          // reverse: window phase (0..half a sine turn)

  int32_t readReverse();
};
