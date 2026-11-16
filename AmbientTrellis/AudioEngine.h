// AudioEngine - FM pad voices. Runs on core 1.
//
// Each voice = one modulator + two slightly detuned carriers (left / right).
// Everything per-sample is integer math; floats are only used when a note starts.
#pragma once
#include <stdint.h>
#include "Config.h"
#include "EventQueue.h"
#include "Reverb.h"
#include "Rng.h"
#include "SoftComp.h"

enum : uint8_t { EV_NOTE_ON = 1, EV_NOTE_OFF = 2, EV_ALL_OFF = 3 };

inline uint32_t packEvent(uint8_t cmd, uint8_t note = 0, uint8_t vel = 0) {
  return ((uint32_t)cmd << 16) | ((uint32_t)note << 8) | vel;
}

// Sound settings. Written by core 0, read by core 1 once per block.
struct PadParams {
  volatile uint32_t attackMs   = 7000;    // chord swell time
  volatile uint32_t releaseMs  = 7500;    // chord fade time
  volatile uint32_t evolveMs   = 14000;   // how long brightness keeps rising
  volatile uint32_t lfoMs      = 24000;   // period of the slow LFO
  volatile uint32_t brightness = 22000;   // max FM index (0..52000)
  volatile uint32_t volume     = 225;     // master volume, 0..256
  volatile uint32_t reverbWet  = 150;     // 0 = dry .. 256 = fully wet
  volatile uint32_t reverbDecay = 27500;  // tail length, Q15 (max ~30000)
};

class AudioEngine {
 public:
  void init(uint32_t seed);

  // Render `frames` (must be cfg::BLOCK) of interleaved stereo audio.
  void render(int16_t *out, int frames);

  EventQueue<64> events;   // core 0 -> core 1
  PadParams params;

  // For the LEDs later: which note each voice plays and how loud it is.
  volatile uint8_t  voiceNote[cfg::NUM_VOICES] = {};
  volatile uint16_t voiceLevel[cfg::NUM_VOICES] = {};   // 0 = silent

 private:
  enum : uint8_t { IDLE = 0, ON = 1, RELEASE = 2 };

  struct Voice {
    uint32_t phM, phL, phR;          // phases
    uint32_t incM, incL, incR;       // base phase increments
    uint32_t curL, curR;             // increments incl. detune wobble
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

  void handleEvent(uint32_t ev);
  void noteOn(uint8_t note, uint8_t vel);
  void noteOff(uint8_t note);
  void controlUpdate();

  Voice v_[cfg::NUM_VOICES];
  Rng rng_;
  SoftComp comp_;
  Reverb reverb_;
  uint32_t lfoPh_ = 0, lfo2Ph_ = 0;
  int32_t mixL_[cfg::BLOCK];
  int32_t mixR_[cfg::BLOCK];
  int32_t mix_[cfg::BLOCK * 2];
};
