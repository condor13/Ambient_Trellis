// Motion - turns accelerometer readings into tilt and shake. Core 0.
//
// Tilt: how far the box leans, whatever the direction (so it doesn't matter
// which way the sensor is glued). Smoothed: about half a second to follow.
//   flat           -> 0    calm: chords stay near home, plucks move in small steps
//   ~40 degrees+   -> 1    adventurous: chords wander, plucks leap
//
// Shake: a few strong jolts within a moment. A single bump (like pressing a
// pad) is not enough. After a shake there's a short pause before the next.
//
// Plain logic: give it readings in milli-g (1000 = gravity), ~50 per second.
#pragma once
#include <stdint.h>

class Motion {
 public:
  // Returns true when a shake has just been detected.
  bool update(int32_t ax, int32_t ay, int32_t az, uint32_t nowMs);

  float tilt() const { return tilt_; }        // 0 (flat) .. 1 (tilted)

  // What the tilt means for the music
  uint8_t drift() const;                      // 2 .. 16
  uint8_t leap() const;                       // 7 .. 12 semitones

 private:
  float lean_ = 0;           // smoothed sine of the lean angle
  float tilt_ = 0;
  float energy_ = 0;         // recent shaking
  uint32_t quietUntil_ = 0;  // no new shake before this time
  bool first_ = true;
};
