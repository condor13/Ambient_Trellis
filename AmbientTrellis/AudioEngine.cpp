#include "AudioEngine.h"
#include <math.h>

// Keep the hot audio code in RAM on the Pico (flash is slower).
#if defined(ARDUINO_ARCH_RP2040)
#include <pico/platform.h>
#define AUDIO_FAST(f) __not_in_flash_func(f)
#else
#define AUDIO_FAST(f) f
#endif

// 2048-point sine table, built once in RAM. Index = phase >> 21.
static int16_t sineTab[2048];

static inline int32_t sine(uint32_t phase) { return sineTab[phase >> 21]; }

static constexpr int32_t ENV_MAX = (1 << 24) - 256;   // lvl full scale (Q24); >>8 stays <= 65535
static constexpr int32_t IDX_MIN = 2500;      // FM index at the very start (almost a sine)

void AudioEngine::init(uint32_t seed) {
  for (int i = 0; i < 2048; i++) {
    sineTab[i] = (int16_t)lrintf(32767.0f * sinf(6.2831853f * (float)i / 2048.0f));
  }
  rng_.seed(seed);
  comp_.setup(16000, 3, 9);
  for (auto &v : v_) {
    v = Voice{};
    v.state = IDLE;
  }
}

void AudioEngine::handleEvent(uint32_t ev) {
  uint8_t cmd  = (ev >> 16) & 0xff;
  uint8_t note = (ev >> 8) & 0xff;
  uint8_t vel  = ev & 0xff;
  switch (cmd) {
    case EV_NOTE_ON:  noteOn(note, vel); break;
    case EV_NOTE_OFF: noteOff(note); break;
    case EV_ALL_OFF:
      for (auto &v : v_) if (v.state == ON) v.state = RELEASE;
      break;
  }
}

void AudioEngine::noteOn(uint8_t note, uint8_t vel) {
  // Already sounding (or fading)? Then just bring it back up smoothly.
  for (auto &v : v_) {
    if (v.state != IDLE && v.note == note) {
      v.state = ON;
      v.velGain = (uint16_t)(vel * 516);
      return;
    }
  }

  // Find a free voice, otherwise steal the quietest one (preferably fading).
  Voice *pick = nullptr;
  for (auto &v : v_) if (v.state == IDLE) { pick = &v; break; }
  if (!pick) {
    int32_t best = INT32_MAX;
    for (int pass = 0; pass < 2 && !pick; pass++) {
      for (auto &v : v_) {
        if (pass == 0 && v.state != RELEASE) continue;
        if (v.lvl < best) { best = v.lvl; pick = &v; }
      }
    }
  }
  Voice &v = *pick;

  float freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
  float inc  = freq * (4294967296.0f / (float)cfg::SAMPLE_RATE);
  // About ±4 cents between left and right for width. Bass notes stay almost
  // centred, because very wide bass feels odd on headphones.
  const bool bass = note < 48;
  const float detune = bass ? 0.0005f : 0.0023f;

  v.incL = (uint32_t)(inc * (1.0f - detune));
  v.incR = (uint32_t)(inc * (1.0f + detune));
  // The modulator runs a tiny bit off the carrier (0.05-0.12 Hz), so the
  // timbre slowly turns over by itself.
  v.incM = (uint32_t)inc + (uint32_t)rng_.range(6700, 16000);
  v.curL = v.incL;
  v.curR = v.incR;

  v.phM = rng_.next();
  v.phL = rng_.next();
  v.phR = bass ? v.phL : rng_.next();
  v.lfoOfs  = rng_.next();
  v.lfo2Ofs = rng_.next();

  v.lvl = 0;
  v.amp = 0;
  v.ampStep = 0;
  v.bloom = 0;
  v.idx = IDX_MIN;
  v.idxStep = 0;
  v.velGain = (uint16_t)(vel * 516);

  // FM amount by register: soft bass, fullest in the middle, gentle on top.
  int ks;
  if (note < 40)      ks = 140;
  else if (note < 48) ks = 140 + (note - 40) * 14;   // up to 252
  else if (note <= 62) ks = 256;
  else if (note < 84) ks = 256 - (note - 62) * 6;    // down to 124
  else                ks = 124;
  v.keyScale = (uint16_t)ks;

  v.note = note;
  v.state = ON;
}

void AudioEngine::noteOff(uint8_t note) {
  for (auto &v : v_) if (v.state == ON && v.note == note) v.state = RELEASE;
}

// Runs once per block (1 kHz): envelopes, brightness, LFOs.
void AudioEngine::controlUpdate() {
  uint32_t atkMs = params.attackMs  ? params.attackMs  : 1;
  uint32_t relMs = params.releaseMs ? params.releaseMs : 1;
  uint32_t evoMs = params.evolveMs  ? params.evolveMs  : 1;
  uint32_t lfoMs = params.lfoMs     ? params.lfoMs     : 1;
  int32_t bright = (int32_t)params.brightness;
  if (bright > 52000) bright = 52000;
  if (bright < IDX_MIN) bright = IDX_MIN;

  int32_t atkInc = ENV_MAX / (int32_t)atkMs;
  int32_t relInc = ENV_MAX / (int32_t)relMs;
  int32_t evoInc = 65536 / (int32_t)evoMs + 1;

  lfoPh_  += 0xFFFFFFFFu / lfoMs;
  lfo2Ph_ += 0xFFFFFFFFu / (lfoMs + lfoMs / 2 + 7000);   // second, slower LFO

  for (int i = 0; i < cfg::NUM_VOICES; i++) {
    Voice &v = v_[i];
    if (v.state == IDLE) {
      voiceLevel[i] = 0;
      continue;
    }

    // Envelope (linear position, curved into amplitude below)
    if (v.state == ON) {
      v.lvl += atkInc;
      if (v.lvl > ENV_MAX) v.lvl = ENV_MAX;
      v.bloom += evoInc;
      if (v.bloom > 65535) v.bloom = 65535;
    } else {
      v.lvl -= relInc;
      if (v.lvl <= 0) {
        v.lvl = 0;
        v.state = IDLE;
        v.amp = 0;
        v.ampStep = 0;
        voiceLevel[i] = 0;
        continue;
      }
    }

    // Amplitude: squared curve = slow start, then rising (a swell)
    uint32_t l16 = (uint32_t)(v.lvl >> 8);                 // Q16
    uint32_t curve = (l16 * l16) >> 16;                     // Q16
    int32_t ampT = (int32_t)((curve * v.velGain) >> 16);    // Q16

    // Brightness: from almost-sine up to `bright` as the chord blooms,
    // breathing ±25% with the slow LFO.
    int32_t lfo1 = sine(lfoPh_ + v.lfoOfs);                 // ±32767
    int32_t idxT = IDX_MIN + (int32_t)(((int64_t)(bright - IDX_MIN) * v.bloom) >> 16);
    idxT = (idxT * v.keyScale) >> 8;
    idxT += (idxT * lfo1) >> 17;
    if (idxT < 0) idxT = 0;

    // Detune wobble, about ±1.5 cents, on the second LFO
    int32_t lfo2 = sine(lfo2Ph_ + v.lfo2Ofs);
    int32_t wob = (int32_t)(((int64_t)(v.incL >> 10) * lfo2) >> 15);
    v.curL = v.incL - (uint32_t)wob;
    v.curR = v.incR + (uint32_t)wob;

    v.ampStep = (ampT - v.amp) >> cfg::BLOCK_SHIFT;
    v.idxStep = (idxT - v.idx) >> cfg::BLOCK_SHIFT;

    voiceNote[i] = v.note;
    voiceLevel[i] = (uint16_t)(ampT > 65535 ? 65535 : ampT);
  }
}

void AUDIO_FAST(AudioEngine::render)(int16_t *out, int frames) {
  uint32_t ev;
  while (events.pop(ev)) handleEvent(ev);

  controlUpdate();

  for (int i = 0; i < frames; i++) mixL_[i] = mixR_[i] = 0;

  for (auto &v : v_) {
    if (v.state == IDLE) continue;
    uint32_t pm = v.phM, pl = v.phL, pr = v.phR;
    const uint32_t im = v.incM, il = v.curL, ir = v.curR;
    int32_t a = v.amp, idx = v.idx;
    const int32_t as = v.ampStep, xs = v.idxStep;

    for (int i = 0; i < frames; i++) {
      int32_t m = sine(pm);
      uint32_t o = (uint32_t)(m * idx);   // phase offset from the modulator
      int32_t sl = sine(pl + o);
      int32_t sr = sine(pr + o);
      mixL_[i] += (sl * a) >> 16;
      mixR_[i] += (sr * a) >> 16;
      pm += im; pl += il; pr += ir;
      a += as; idx += xs;
    }

    v.phM = pm; v.phL = pl; v.phR = pr;
    v.amp = a; v.idx = idx;
  }

  // Master: headroom for ~4 full voices, then volume, then compressor.
  int32_t vol = (int32_t)params.volume;
  for (int i = 0; i < frames; i++) {
    mix_[2 * i]     = ((mixL_[i] >> 2) * vol) >> 8;
    mix_[2 * i + 1] = ((mixR_[i] >> 2) * vol) >> 8;
  }
  comp_.process(mix_, out, frames);
}
