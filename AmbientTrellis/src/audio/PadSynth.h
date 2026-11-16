// PadSynth - the FM chord pads (bass and mid).
//
// Each voice = one modulator + two slightly detuned carriers (left / right).
// Per-sample work is integer only; floats are used only when a note starts.
#pragma once
#include <stdint.h>
#include "../Config.h"
#include "../util/Rng.h"
#include "SoundParams.h"

class PadSynth {
 public:
  void init(uint32_t seed);

  void noteOn(uint8_t note, uint8_t vel);
  void noteOff(uint8_t note);
  void releaseAll();

  // Once per block: envelopes, brightness, LFOs.
  void control(const SoundParams &p);

  // Adds `frames` samples of all voices into L and R.
  void render(int32_t *L, int32_t *R, int frames);

  // For the LEDs: which note each voice plays and how loud it is (0 = silent).
  volatile uint8_t  voiceNote[cfg::NUM_VOICES] = {};
  volatile uint16_t voiceLevel[cfg::NUM_VOICES] = {};

 private:
  enum : uint8_t { IDLE = 0, ON = 1, RELEASE = 2 };

  struct Voice {
    uint32_t phM, phL, phR;          // phases (a full turn = 2^32)
    uint32_t incM, incL, incR;       // phase steps per sample
    uint32_t curL, curR;             // carrier steps incl. detune wobble
    int32_t  lvl;                    // envelope position, Q24
    int32_t  amp, ampStep;           // amplitude Q16, ramped per sample
    int32_t  idx, idxStep;           // FM index, ramped per sample
    int32_t  bloom;                  // brightness rise, Q16
    uint32_t lfoOfs, lfo2Ofs;        // per-voice LFO phase offsets
    uint16_t velGain;                // Q16
    uint16_t keyScale;               // Q8, FM amount by register
    uint8_t  note;
    uint8_t  state;
  };

  Voice v_[cfg::NUM_VOICES];
  Rng rng_;
  uint32_t lfoPh_ = 0, lfo2Ph_ = 0;
};
