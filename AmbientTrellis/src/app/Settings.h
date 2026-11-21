// Settings - the 16 parameters of the SHIFT menu, in one place.
//
// Every parameter is stored as the pad number you pressed: 1..16 (or 1..4 for
// the four-option ones). This file knows what each value means (names, texts
// for the display, and how it maps to the sound) and applies it.
//
// Pad layout (same as the menu grid):
//   PLAY  DENS  ROOT  PVOL
//   VERB  LFO   BPM   BARS
//   ATK   REL   BRIT  VOL
//   DLY   DMOD  LITE  SLEEP
#pragma once
#include <stdint.h>

class AudioEngine;
class Sequencer;
class PondLeds;

enum Param : uint8_t {
  P_PLAY = 0, P_DENS, P_ROOT, P_PVOL,
  P_VERB, P_LFO, P_BPM, P_BARS,
  P_ATK, P_REL, P_BRIT, P_VOL,
  P_DLY, P_DMOD, P_LITE, P_SLEEP,
  P_COUNT
};

struct Settings {
  // value[p] = 1..16 (1..4 for BARS and DMOD). For ROOT it is the root note
  // (1 = C .. 12 = B); the scale is kept separately.
  uint8_t value[P_COUNT];
  uint8_t scale;      // 0 Minor, 1 Dorian, 2 Major, 3 Lydian
  bool    playing;

  void setDefaults();

  // Push one parameter (or all of them) to the sound and lights.
  void apply(Param p, AudioEngine &eng, Sequencer &seq, PondLeds &pond, uint32_t nowMs) const;
  void applyAll(AudioEngine &eng, Sequencer &seq, PondLeds &pond, uint32_t nowMs) const;

  // For the menu
  static uint8_t options(Param p);           // 16, or 4 for BARS / DMOD
  static const char *shortName(Param p);     // "VERB"   (menu grid)
  static const char *longName(Param p);      // "REVERB" (value page, overlay)
  void valueText(Param p, char *buf, int size) const;   // "60 BPM", "D DORIAN" ...

  // Saving: the settings as a few bytes (with a check), and back.
  // unpack() returns false (and changes nothing) if the bytes are not valid
  // saved settings, e.g. on the very first start.
  static constexpr int PACKED_SIZE = 24;
  void pack(uint8_t *out) const;
  bool unpack(const uint8_t *in);

  // LED master brightness from LITE (0..255)
  uint8_t ledBrightness() const;
  // SLEEP in minutes, 0 = never
  uint32_t sleepMinutes() const;
};
