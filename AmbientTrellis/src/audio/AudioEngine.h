// AudioEngine - runs on core 1. Receives events, runs the instruments and
// effects, and mixes them into the final stereo signal.
//
//   pads ----------------------------+--> main ---+
//                                    |            |
//   plucks --+-- (soft) ------------>|            +--> compressor --> DAC
//            +-- (strong) --> send --+--> reverb -+
//            +--> delay -------------+
//
// "main" is what you hear directly, "send" is what goes into the reverb.
#pragma once
#include <stdint.h>
#include "../Config.h"
#include "../util/EventQueue.h"
#include "Events.h"
#include "PadSynth.h"
#include "PluckDelay.h"
#include "PluckSynth.h"
#include "Reverb.h"
#include "SoftComp.h"
#include "SoundParams.h"

class AudioEngine {
 public:
  void init(uint32_t seed);

  // Core 1: render `frames` (must be cfg::BLOCK) of interleaved stereo audio.
  void render(int16_t *out, int frames);

  // Core 0: start a pluck. The slow float math happens here, on core 0;
  // core 1 only receives the finished recipe.
  // tag: 0 = generative, 1..16 = started by touching pad (tag-1).
  bool sendPluck(uint8_t note, uint8_t vel, uint8_t tag = 0);

  EventQueue<64> events;   // core 0 -> core 1: pad notes
  SoundParams params;      // core 0 writes, core 1 reads

  PadSynth pads;           // public so core 0 can read the levels for the LEDs
  PluckSynth plucks;

 private:
  void handleEvent(uint32_t ev);

  EventQueue<8, PluckSpec> pluckQueue_;   // core 0 -> core 1: pluck recipes
  Rng designRng_;                         // used on core 0 only (pluck pan)
  int32_t plkLp_[4] = {};                 // pluck high-cut filter memory
  PluckDelay delay_;
  Reverb reverb_;
  SoftComp comp_;

  // Work buffers for one block
  int32_t padL_[cfg::BLOCK], padR_[cfg::BLOCK];
  int32_t plkL_[cfg::BLOCK], plkR_[cfg::BLOCK];
  int32_t plk_[cfg::BLOCK * 2];    // plucks, interleaved (delay input)
  int32_t echo_[cfg::BLOCK * 2];   // delay output
  int32_t main_[cfg::BLOCK * 2];   // heard directly
  int32_t send_[cfg::BLOCK * 2];   // into the reverb
};
