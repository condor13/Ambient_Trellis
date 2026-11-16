// Optimize this file for speed (the Arduino-Pico default is size).
#pragma GCC optimize("O2")

#include "PluckDelay.h"
#include "../util/FastCode.h"

static ALWAYS_INLINE int16_t sat16(int32_t x) {
  return (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
}

void PluckDelay::init() {
  for (int32_t i = 0; i < MAX; i++) bufL_[i] = bufR_[i] = 0;
  pos_ = 0;
  lp_ = lastL_ = lastR_ = 0;
}

void PluckDelay::setTime(uint32_t samples32k) {
  int32_t n = (int32_t)(samples32k / 2);
  if (n < 1) n = 1;
  if (n > MAX - 1) n = MAX - 1;
  len_ = n;
}

void AUDIO_FAST(PluckDelay::process)(const int32_t *in, int32_t *out, int frames) {
  const int32_t fb = feedback, damp = damping;

  for (int i = 0; i < frames; i += 2) {
    // Two 32 kHz frames -> one 16 kHz mono sample
    int32_t x = (in[2 * i] + in[2 * i + 1] + in[2 * i + 2] + in[2 * i + 3]) >> 2;

    int32_t rd = pos_ - len_;
    if (rd < 0) rd += MAX;
    int32_t dl = bufL_[rd];
    int32_t dr = bufR_[rd];

    // Ping-pong: input + feedback -> left line, left line -> right line.
    lp_ += ((dr - lp_) * damp) >> 15;
    bufL_[pos_] = sat16(x + ((lp_ * fb) >> 15));
    bufR_[pos_] = sat16(dl);
    if (++pos_ >= MAX) pos_ = 0;

    // Back to 32 kHz: first frame halfway between, second frame the new value
    out[2 * i]     = (lastL_ + dl) >> 1;
    out[2 * i + 1] = (lastR_ + dr) >> 1;
    out[2 * i + 2] = dl;
    out[2 * i + 3] = dr;
    lastL_ = dl;
    lastR_ = dr;
  }
}
