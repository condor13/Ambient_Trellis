// SettingsStore - keeps the settings in flash, so the box starts the next day
// as you left it. Core 0.
//
// Uses the Pico core's EEPROM emulation (one 4 KB flash sector at the end of
// the flash). Writing flash pauses both cores for a moment (~45 ms), so the
// app only saves a few seconds after the last change, with the menu closed,
// and the audio buffer is big enough to play on through the pause.
#pragma once
#include <stdint.h>
#include "../app/Settings.h"

class SettingsStore {
 public:
  void begin();
  bool load(Settings &s);          // false: nothing valid saved (defaults stay)
  bool save(const Settings &s);    // writes only if something changed

 private:
  uint8_t last_[Settings::PACKED_SIZE] = {};
};
