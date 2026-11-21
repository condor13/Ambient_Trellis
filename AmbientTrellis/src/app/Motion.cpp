#include "Motion.h"
#include <math.h>

bool Motion::update(int32_t ax, int32_t ay, int32_t az, uint32_t nowMs) {
  float x = (float)ax, y = (float)ay, z = (float)az;
  float total = sqrtf(x * x + y * y + z * z);         // ~1000 when still
  if (total < 1.0f) return false;

  // Tilt. Lying flat, gravity points straight down through the board (z).
  // The sideways part of gravity, compared to all of it, is the sine of the
  // lean angle: 0 when flat, 0.5 at 30 degrees, 0.71 at 45.
  float lean = sqrtf(x * x + y * y) / total;
  if (first_) { lean_ = lean; first_ = false; }
  lean_ += (lean - lean_) * 0.04f;                    // ~0.5 s to follow at 50 readings/s
  float t = (lean_ - 0.08f) / 0.56f;                  // ~5 degrees dead zone, full at ~40
  tilt_ = t < 0 ? 0 : (t > 1 ? 1 : t);

  // Shake. Still or slowly tilting, the total stays near 1 g. Jolts push it
  // far above or below. Sum the jolts with a short memory: one bump fades
  // away, several in a row add up past the threshold.
  float jolt = fabsf(total - 1000.0f);
  if (jolt < 250.0f) jolt = 0;                        // ignore small knocks
  energy_ = energy_ * 0.85f + jolt;
  if (energy_ > 2600.0f && (int32_t)(nowMs - quietUntil_) >= 0) {
    energy_ = 0;
    quietUntil_ = nowMs + 2500;
    return true;
  }
  return false;
}

uint8_t Motion::drift() const { return (uint8_t)(2.5f + tilt_ * 13.5f); }
uint8_t Motion::leap() const { return (uint8_t)(7.5f + tilt_ * 5.0f); }
