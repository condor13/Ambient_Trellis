// SoftComp - gentle stereo compressor followed by a soft clipper.
//
// Self-contained and integer-only, so it can be reused in other RP2040
// projects (for example the Pico step sequencer).
//
// Input:  interleaved stereo int32 where 32767 = full scale. Louder input is
//         allowed (up to about 4x full scale).
// Output: interleaved stereo int16, never clips hard.
//
// The compressor works per block: it follows the block peak with an instant
// attack and a slow release, and reduces anything above the threshold with a
// soft ratio. The soft clipper then rounds off whatever is still too loud.
#pragma once
#include <stdint.h>

class SoftComp {
 public:
  // threshold: level where compression starts (32767 = full scale)
  // ratio:     2 = gentle, 4 = firm
  // releaseShift: higher = slower release (9 ≈ 0.5 s at 1 kHz block rate)
  void setup(int32_t threshold = 16000, int32_t ratio = 3, int releaseShift = 9) {
    thr_ = threshold;
    ratio_ = ratio < 1 ? 1 : ratio;
    relShift_ = releaseShift;
  }

  void process(const int32_t *in, int16_t *out, int frames) {
    // 1. Block peak
    int32_t peak = 0;
    for (int i = 0; i < frames * 2; i++) {
      int32_t a = in[i] < 0 ? -in[i] : in[i];
      if (a > peak) peak = a;
    }

    // 2. Envelope: instant attack, slow release
    if (peak > env_) env_ = peak;
    else             env_ -= (env_ - peak) >> relShift_;

    // 3. Target gain (Q14, 16384 = unity)
    int32_t target = 16384;
    if (env_ > thr_) {
      int32_t wanted = thr_ + (env_ - thr_) / ratio_;
      target = (int32_t)(((int64_t)wanted << 14) / env_);
    }

    // 4. Apply gain (ramped across the block) and soft clip
    int32_t g = gain_;
    int32_t step = (target - g) / frames;
    for (int i = 0; i < frames; i++) {
      g += step;
      out[2 * i]     = softClip(mulGain(in[2 * i], g));
      out[2 * i + 1] = softClip(mulGain(in[2 * i + 1], g));
    }
    gain_ = target;
  }

  // Current gain reduction, 16384 = none. Handy for a meter.
  int32_t gain() const { return gain_; }

 private:
  static int32_t mulGain(int32_t x, int32_t g) {
    if (x > 131071) x = 131071;          // keeps x * g inside int32
    if (x < -131071) x = -131071;
    return (x * g) >> 14;
  }

  // Linear up to KNEE, then bends smoothly towards full scale.
  static int16_t softClip(int32_t x) {
    const int32_t KNEE = 22000;
    const int32_t HEAD = 32767 - KNEE;
    int32_t a = x < 0 ? -x : x;
    if (a > KNEE) {
      int32_t e = a - KNEE;
      a = KNEE + (e * HEAD) / (e + HEAD);
    }
    return (int16_t)(x < 0 ? -a : a);
  }

  int32_t thr_ = 16000;
  int32_t ratio_ = 3;
  int relShift_ = 9;
  int32_t env_ = 0;
  int32_t gain_ = 16384;
};
