// Events: the messages core 0 sends to the audio engine on core 1.
// Each event is packed into one 32-bit word: [cmd:8][note:8][vel:8].
#pragma once
#include <stdint.h>

enum : uint8_t {
  EV_NOTE_ON  = 1,   // pad note starts swelling in
  EV_NOTE_OFF = 2,   // pad note starts fading out
  EV_ALL_OFF  = 3,   // all pad notes fade out
  EV_PLUCK    = 4,   // one plucked string note
};

inline uint32_t packEvent(uint8_t cmd, uint8_t note = 0, uint8_t vel = 0) {
  return ((uint32_t)cmd << 16) | ((uint32_t)note << 8) | vel;
}

inline uint8_t eventCmd(uint32_t ev)  { return (ev >> 16) & 0xff; }
inline uint8_t eventNote(uint32_t ev) { return (ev >> 8) & 0xff; }
inline uint8_t eventVel(uint32_t ev)  { return ev & 0xff; }
