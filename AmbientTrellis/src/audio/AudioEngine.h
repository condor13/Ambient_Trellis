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

  // Render `frames` (must be cfg::BLOCK) of interleaved stereo audio.
  void render(int16_t *out, int frames);

  EventQueue<64> events;   // core 0 -> core 1
  SoundParams params;      // core 0 writes, core 1 reads

  PadSynth pads;           // public so core 0 can read the LED levels
  PluckSynth plucks;

 private:
  void handleEvent(uint32_t ev);

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
