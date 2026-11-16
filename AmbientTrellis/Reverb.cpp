#include "Reverb.h"
#include "FastCode.h"

// Delay lengths from Dattorro's plate (designed at 29761 Hz), scaled to 32 kHz.
namespace {
constexpr int32_t PRE = 800;                      // 25 ms pre-delay
constexpr int32_t AP1 = 153, AP2 = 115, AP3 = 408, AP4 = 298;
constexpr int32_t EXC = 17;                       // tank modulation depth (samples)
constexpr int32_t L_AP = 723, L_D1 = 4788, L_AP2 = 1935, L_D2 = 4000;
constexpr int32_t R_AP = 976, R_D1 = 4534, R_AP2 = 2856, R_D2 = 3401;

int16_t bPre[PRE], bAp1[AP1], bAp2[AP2], bAp3[AP3], bAp4[AP4];
int16_t bLAp[L_AP + EXC + 2], bLD1[L_D1], bLAp2[L_AP2], bLD2[L_D2];
int16_t bRAp[R_AP + EXC + 2], bRD1[R_D1], bRAp2[R_AP2], bRD2[R_D2];

// Coefficients, Q15
constexpr int32_t IN_DIFF1 = 24576;     // 0.75
constexpr int32_t IN_DIFF2 = 20480;     // 0.625
constexpr int32_t DEC_DIFF1 = -22938;   // -0.70 (modulated tank allpass)
constexpr int32_t DEC_DIFF2 = 16384;    // 0.50

// Wet level so that a fully wet signal is about as loud as the dry one.
constexpr int32_t OUT_GAIN = 2;

// Smooth-ish LFO shape (parabolic sine), ±32767
int32_t lfoShape(uint32_t ph) {
  int32_t p = (int32_t)(ph >> 16);
  int32_t half = p & 0x7FFF;
  int32_t y = (half * (32768 - half)) >> 13;
  if (y > 32767) y = 32767;
  return (p & 0x8000) ? -y : y;
}
}  // namespace

void Reverb::init() {
  auto setup = [](Line &l, int16_t *buf, int32_t len) {
    l.buf = buf;
    l.len = len;
    l.pos = 0;
    for (int32_t i = 0; i < len; i++) buf[i] = 0;
  };
  setup(pre_, bPre, PRE);
  setup(ap1_, bAp1, AP1);
  setup(ap2_, bAp2, AP2);
  setup(ap3_, bAp3, AP3);
  setup(ap4_, bAp4, AP4);
  setup(lAp_, bLAp, L_AP + EXC + 2);
  setup(lD1_, bLD1, L_D1);
  setup(lAp2_, bLAp2, L_AP2);
  setup(lD2_, bLD2, L_D2);
  setup(rAp_, bRAp, R_AP + EXC + 2);
  setup(rD1_, bRD1, R_D1);
  setup(rAp2_, bRAp2, R_AP2);
  setup(rD2_, bRD2, R_D2);
  bw_ = lDamp_ = rDamp_ = lFeed_ = rFeed_ = 0;
}

inline int32_t Reverb::allpass(Line &l, int32_t x, int32_t g) {
  int32_t d = l.read(l.len);
  int32_t v = x - ((g * d) >> 15);
  l.write(v);
  return d + ((g * v) >> 15);
}

inline int32_t Reverb::allpassMod(Line &l, int32_t x, int32_t g, int32_t delayQ16) {
  int32_t di = delayQ16 >> 16;
  int32_t f = (delayQ16 & 0xFFFF) >> 1;              // Q15 fraction
  int32_t a = l.read(di);
  int32_t b = l.read(di + 1);
  int32_t d = a + (((b - a) * f) >> 15);
  int32_t v = x - ((g * d) >> 15);
  l.write(v);
  return d + ((g * v) >> 15);
}

void AUDIO_FAST(Reverb::process)(int32_t *mix, int frames, int32_t wet, int32_t dry) {
  // Slow modulation of the two tank allpasses (about 0.6 Hz), once per block
  lfo_ += 2576980u * (uint32_t)frames / 32u;
  int32_t lDel = (L_AP << 16) + lfoShape(lfo_) * EXC * 2;
  int32_t rDel = (R_AP << 16) + lfoShape(lfo_ + 0x40000000u) * EXC * 2;

  const int32_t dec = decay, damp = damping, bw = inputBw;

  for (int i = 0; i < frames; i++) {
    int32_t inL = mix[2 * i], inR = mix[2 * i + 1];

    // Mono input with headroom, clamped to the 16-bit delay lines
    int32_t x = (inL + inR) >> 2;
    if (x > 32767) x = 32767;
    if (x < -32768) x = -32768;

    int32_t p = pre_.read(PRE);
    pre_.write(x);

    // Input low-pass and diffusion
    bw_ += ((p - bw_) * bw) >> 15;
    int32_t d = allpass(ap1_, bw_, IN_DIFF1);
    d = allpass(ap2_, d, IN_DIFF1);
    d = allpass(ap3_, d, IN_DIFF2);
    d = allpass(ap4_, d, IN_DIFF2);

    // Left tank
    int32_t t = allpassMod(lAp_, d + rFeed_, DEC_DIFF1, lDel);
    int32_t o = lD1_.read(L_D1);
    lD1_.write(t);
    lDamp_ += ((o - lDamp_) * damp) >> 15;
    t = allpass(lAp2_, (lDamp_ * dec) >> 15, DEC_DIFF2);
    o = lD2_.read(L_D2);
    lD2_.write(t);
    int32_t lOut = (o * dec) >> 15;

    // Right tank
    t = allpassMod(rAp_, d + lFeed_, DEC_DIFF1, rDel);
    o = rD1_.read(R_D1);
    rD1_.write(t);
    rDamp_ += ((o - rDamp_) * damp) >> 15;
    t = allpass(rAp2_, (rDamp_ * dec) >> 15, DEC_DIFF2);
    o = rD2_.read(R_D2);
    rD2_.write(t);
    int32_t rOut = (o * dec) >> 15;

    lFeed_ = lOut;
    rFeed_ = rOut;

    // Output taps (Dattorro's stereo pick-up points)
    int32_t yl = rD1_.read(286) + rD1_.read(3198) - rAp2_.read(2057) + rD2_.read(2146)
               - lD1_.read(2140) - lAp2_.read(201) - lD2_.read(1146);
    int32_t yr = lD1_.read(380) + lD1_.read(3900) - lAp2_.read(1320) + lD2_.read(2874)
               - rD1_.read(2270) - rAp2_.read(360) - rD2_.read(130);
    yl = ((yl >> 2) * 19661) >> 13;   // * 0.6
    yr = ((yr >> 2) * 19661) >> 13;

    mix[2 * i]     = ((inL * dry) >> 8) + ((yl * OUT_GAIN * wet) >> 8);
    mix[2 * i + 1] = ((inR * dry) >> 8) + ((yr * OUT_GAIN * wet) >> 8);
  }
}
