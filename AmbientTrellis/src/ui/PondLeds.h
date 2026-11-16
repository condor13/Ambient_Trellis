// PondLeds - the "pond of notes" on the 4x4 pads. Runs on core 0.
//
// The 16 pads are 16 places, not fixed keys. Every sounding note takes a
// random free pad and glows in its note colour, following the actual loudness
// of the sound. When a chord changes, the old notes fade out where they are and
// the new ones appear on free pads.
//
// - Pad chord notes: note colour (C red, D green, E yellow, F orange, G blue,
//   A turquoise, B pink; sharps are a mix of their two neighbours).
// - Generative plucks: a short glow in the note colour on a free pad.
// - Touch plucks: the touched pad itself glows water blue.
//
// Plain C++ with no hardware access: it only computes 16 colours, so it can be
// tested on a computer.
#pragma once
#include <stdint.h>
#include "../util/Rng.h"

struct Rgb {
  uint8_t r, g, b;
};

class PondLeds {
 public:
  static constexpr int PADS = 16;
  static constexpr int PAD_SOURCES = 16;     // pad synth voices
  static constexpr int PLUCK_SOURCES = 4;    // pluck strings

  void begin(uint32_t seed, uint32_t nowMs);

  // What is sounding right now (copied from the audio engine).
  struct Input {
    uint8_t  padNote[PAD_SOURCES];
    uint16_t padLevel[PAD_SOURCES];      // 0 = silent .. ~48000
    uint8_t  pluckNote[PLUCK_SOURCES];
    uint16_t pluckLevel[PLUCK_SOURCES];  // 0 = silent .. ~20000
    uint8_t  pluckTag[PLUCK_SOURCES];    // 0 = generative, 1..16 = touched pad + 1
  };

  // Call ~30x per second. Fills out[16] with the colours to show.
  void update(const Input &in, uint32_t nowMs, Rgb *out);

  // Overall LED brightness, 0..255 (the LITE setting later).
  uint8_t brightness = 110;

  static Rgb noteColour(uint8_t note);

 private:
  struct Slot {
    int8_t  src;        // -1 = free, 0..15 pad voice, 16..19 pluck string
    uint8_t note;
    bool    gone;       // the source stopped: fade out, then free
    uint8_t shown;      // displayed brightness 0..255 (smoothed)
  };

  int  findSlot(int src, uint8_t note) const;
  int  freeSlot();
  void place(int src, uint8_t note, uint8_t target);

  Slot slots_[PADS];
  uint8_t touchShown_[PADS] = {};
  Rng rng_;
  uint32_t startMs_ = 0;
};
