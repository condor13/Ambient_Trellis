// Optimize this file for speed (the Arduino-Pico default is size).
#pragma GCC optimize("O2")

#include "PluckDelay.h"
#include "../util/FastCode.h"
#include "../util/SineTable.h"

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

// Reverse: a reader that walks backwards through the last `len` samples while
// the writer walks forwards, so the distance between them grows by 2 per
// sample. When it has covered the whole delay, it jumps back and starts the
// next piece. A smooth window (half a sine) fades every piece in and out, so
// the jumps can't click, and each echo swells in.
ALWAYS_INLINE int32_t PluckDelay::readReverse() {
  int32_t rd = pos_ - revOfs_;
  if (rd < 0) rd += MAX;
  int32_t x = bufL_[rd];
  int32_t win = sineLookup(revPh_ >> 1);          // 0 .. 32767 .. 0
  revOfs_ += 2;
  revPh_ += 0xFFFFFFFFu / (uint32_t)len_ * 2u;
  if (revOfs_ >= len_) {
    revOfs_ = 1;
    revPh_ = 0;
  }
  return (x * win) >> 15;
}

void AUDIO_FAST(PluckDelay::process)(const int32_t *in, int32_t *out, int frames) {
  const int32_t fb = feedback, damp = damping, m = mode;

  for (int i = 0; i < frames; i += 2) {
    // Two 32 kHz frames -> one 16 kHz mono sample
    int32_t x = (in[2 * i] + in[2 * i + 1] + in[2 * i + 2] + in[2 * i + 3]) >> 2;

    int32_t rd = pos_ - len_;
    if (rd < 0) rd += MAX;

    // Line A (bufL_) always takes the input + feedback. What comes out, and
    // what goes on into line B (bufR_), depends on the mode.
    int32_t outL, outR, back;
    if (m == FORWARD) {
      outL = outR = back = bufL_[rd];
    } else if (m == PING_PONG) {
      outL = bufL_[rd];
      outR = back = bufR_[rd];
      bufR_[pos_] = sat16(outL);
    } else if (m == REVERSE) {
      outL = outR = back = readReverse();
    } else {                                      // REVERSE_PING
      outL = readReverse();
      outR = back = bufR_[rd];
      bufR_[pos_] = sat16(outL);
    }

    lp_ += ((back - lp_) * damp) >> 15;           // darker every repeat
    bufL_[pos_] = sat16(x + ((lp_ * fb) >> 15));
    if (++pos_ >= MAX) pos_ = 0;

    // Back to 32 kHz: first frame halfway between, second frame the new value
    out[2 * i]     = (lastL_ + outL) >> 1;
    out[2 * i + 1] = (lastR_ + outR) >> 1;
    out[2 * i + 2] = outL;
    out[2 * i + 3] = outR;
    lastL_ = outL;
    lastR_ = outR;
  }
}
