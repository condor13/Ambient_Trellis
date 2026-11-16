// PluckSynth - soft, high plucked strings (Karplus-Strong).
//
// A string is a short delay line, as long as one period of the note. A burst
// of soft noise is put in, then it circulates: every round trip it is gently
// low-passed and made a little quieter. That loop rings at the note's pitch,
// and the high harmonics die first, just like a real string.
#pragma once
#include <stdint.h>
#include "../util/Rng.h"

class PluckSynth {
 public:
  static constexpr int VOICES = 4;

  void init(uint32_t seed);

  // Start a pluck. note = MIDI note (60..100), vel = 1..127.
  void pluck(uint8_t note, uint8_t vel, uint32_t decayMs);

  // Adds `frames` samples of all strings into L and R.
  void render(int32_t *L, int32_t *R, int frames);

  // For the LEDs: which note each string plays and how loud it is (0 = silent).
  volatile uint8_t  voiceNote[VOICES] = {};
  volatile uint16_t voiceLevel[VOICES] = {};

 private:
  static constexpr int LEN = 128;    // delay line length (enough down to middle C)

  struct Voice {
    int32_t  buf[LEN];     // samples x16 (4 extra bits of precision)
    uint32_t w;            // write position
    int32_t  n;            // whole-sample loop delay
    int32_t  c;            // fractional-delay allpass coefficient, Q10
    int32_t  apX, apY;     // allpass memory
    int32_t  a;            // loop low-pass: a*now + (1-a)*previous, Q8
    int32_t  loss;         // fraction lost per round trip, Q15
    int32_t  prev;         // previous output sample (for the low-pass)
    int32_t  gL, gR;       // pan gains, Q8
    uint32_t age;          // blocks since the pluck
    int32_t  quiet;        // blocks in a row it was almost silent
    uint8_t  note;
    bool     active;
  };

  Voice v_[VOICES];
  Rng rng_;
};
