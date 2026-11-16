// Optimize this file for speed (the Arduino-Pico default is size).
#pragma GCC optimize("O2")

#include "PluckSynth.h"
#include <math.h>
#include "../Config.h"
#include "../util/FastCode.h"

// The string stores its samples with 4 extra bits of precision (x16). Without
// them, the rounding in every round trip eats the quiet end of the tail.
static constexpr int EXTRA = 4;

void PluckSynth::init(uint32_t seed) {
  rng_.seed(seed);
  for (auto &v : v_) {
    v = Voice{};
    v.active = false;
  }
}

void PluckSynth::pluck(uint8_t note, uint8_t vel, uint32_t decayMs) {
  // Free string, otherwise the oldest one.
  Voice *pick = nullptr;
  for (auto &v : v_) if (!v.active) { pick = &v; break; }
  if (!pick) {
    pick = &v_[0];
    for (auto &v : v_) if (v.age > pick->age) pick = &v;
  }
  Voice &v = *pick;

  const float fs = (float)cfg::SAMPLE_RATE;
  float freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
  float period = fs / freq;                     // samples per cycle

  // Ring time: -60 dB after t60 seconds, the same for every note.
  float t60 = (float)decayMs / 1000.0f;
  float perTrip = powf(10.0f, -3.0f / (freq * t60));   // gain per round trip

  // Loop low-pass y = a*x + (1-a)*x_prev: this is what makes the high
  // harmonics die first. High notes go round the loop more often per second,
  // so they need a gentler filter (a closer to 1) to ring as long. We solve
  // for the `a` whose loss at the note's own pitch fits the ring time.
  float w = 6.2831853f * freq / fs;
  float want = perTrip / 0.9990f;                      // leave a little for `decay`
  float k = (1.0f - want * want) / (2.0f * (1.0f - cosf(w)));   // = a(1-a)
  float a = (k < 0.25f) ? 0.5f + sqrtf(0.25f - k) : 0.5f;
  if (a < 0.55f) a = 0.55f;                            // never brighter than needed
  if (a > 0.95f) a = 0.95f;
  float lpGain = sqrtf(1.0f - 2.0f * a * (1.0f - a) * (1.0f - cosf(w)));
  float decay = perTrip / lpGain;
  if (decay > 0.9995f) decay = 0.9995f;

  // Tuning. One trip round the loop must take exactly one period:
  //   whole-sample delay N  +  fractional allpass delay  +  low-pass delay (1-a)
  // The allpass gives the fraction without making anything quieter (reading
  // between two samples would soften the high notes on every trip).
  float loopDelay = period - (1.0f - a);
  int32_t n = (int32_t)(loopDelay - 0.1f);         // keeps the fraction in 0.1..1.1
  if (n < 2) n = 2;
  if (n > LEN - 2) n = LEN - 2;
  float frac = loopDelay - (float)n;
  v.n = n;
  v.c = (int32_t)(1024.0f * (1.0f - frac) / (1.0f + frac));   // allpass coefficient, Q10
  v.apX = v.apY = 0;
  v.a = (int32_t)(a * 256.0f);
  v.loss = (int32_t)((1.0f - decay) * 32768.0f) + 1;
  if (v.loss > 2000) v.loss = 2000;            // keeps the math inside 32 bits

  // Excitation: mostly one smooth sine cycle of the note (a soft, round
  // tone) plus a little low-passed noise (the "string" character).
  const float amp = (float)vel * 150.0f;        // vel 64 -> ~9600
  int32_t lp = 0;
  for (int i = 0; i < LEN; i++) {
    int32_t n = (int32_t)(rng_.next() >> 17) - 16384;     // ±16384
    lp += ((n - lp) * 6000) >> 15;                          // one-pole low-pass
    float tone = sinf(6.2831853f * (float)i / period);
    float x = amp * (0.75f * tone + 0.25f * (float)lp / 6000.0f);
    int32_t s = (int32_t)x;
    v.buf[i] = (s > 30000 ? 30000 : (s < -30000 ? -30000 : s)) << EXTRA;
  }
  // Remove any offset (DC) from the part the string actually plays: the
  // newest one-period-plus-one samples. An offset would not ring, it would
  // just sit there and slowly leak away.
  int32_t span = n + 2, sum = 0;
  for (int i = LEN - span; i < LEN; i++) sum += v.buf[i];
  int32_t dc = sum / span;
  for (int i = 0; i < LEN; i++) v.buf[i] -= dc;

  // Random place in the stereo field (constant loudness)
  float pan = 0.2f + 0.6f * (float)(rng_.next() >> 8) / 16777216.0f;
  v.gL = (int32_t)(256.0f * sqrtf(1.0f - pan));
  v.gR = (int32_t)(256.0f * sqrtf(pan));

  v.w = 0;
  v.prev = 0;
  v.age = 0;
  v.quiet = 0;
  v.note = note;
  v.active = true;
}

void AUDIO_FAST(PluckSynth::render)(int32_t *L, int32_t *R, int frames) {
  for (int k = 0; k < VOICES; k++) {
    Voice &v = v_[k];
    if (!v.active) {
      voiceLevel[k] = 0;
      continue;
    }

    const int32_t n = v.n, c = v.c;
    const int32_t a = v.a, loss = v.loss;
    const int32_t gL = v.gL, gR = v.gR;
    uint32_t w = v.w;
    int32_t prev = v.prev, apX = v.apX, apY = v.apY;
    int32_t peak = 0;

    for (int i = 0; i < frames; i++) {
      // Read one period back (whole samples) ...
      int32_t s = v.buf[(w - n) & (LEN - 1)];
      // ... plus the fraction, with a first-order allpass:
      //   x[n] = c*(s[n] - x[n-1]) + s[n-1]
      int32_t x = ((c * (s - apY)) >> 10) + apX;
      apX = s;
      apY = x;

      // Low-pass: y = a*x + (1-a)*previous
      int32_t y = prev + (((x - prev) * a) >> 8);
      prev = x;

      // Loss. At least 3 tiny steps (3/16 of a sample unit) per trip: more
      // than the rounding in the filters can ever add back, so the string
      // keeps getting quieter until it is truly silent instead of humming.
      int32_t m = y < 0 ? -y : y;
      int32_t l = (m * loss) >> 15;
      if (l < 3) l = 3;
      if (l > m) l = m;
      y = y < 0 ? y + l : y - l;

      v.buf[w & (LEN - 1)] = y;
      w++;

      int32_t out = x >> EXTRA;
      L[i] += (out * gL) >> 8;
      R[i] += (out * gR) >> 8;
      int32_t ao = out < 0 ? -out : out;
      if (ao > peak) peak = ao;
    }

    v.w = w;
    v.prev = prev;
    v.apX = apX;
    v.apY = apY;
    v.age++;
    voiceNote[k] = v.note;
    voiceLevel[k] = (uint16_t)(peak * 2 > 65535 ? 65535 : peak * 2);

    // Silent for 50 ms in a row: the string is done.
    if (peak < 6) {
      if (++v.quiet > 50) {
        v.active = false;
        voiceLevel[k] = 0;
      }
    } else {
      v.quiet = 0;
    }
  }
}
