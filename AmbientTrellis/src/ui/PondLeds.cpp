#include "PondLeds.h"
#include "Colour.h"
#include <math.h>

namespace {

// Note colours. Naturals as chosen; sharps are a mix of their two neighbours.
const Rgb NATURAL[7] = {
  {255,   0,   0},   // C red
  {  0, 255,   0},   // D green
  {255, 190,   0},   // E yellow
  {255,  80,   0},   // F orange
  {  0,  40, 255},   // G blue
  {  0, 255, 170},   // A turquoise
  {255,  50, 150},   // B pink
};
// For each pitch class: the natural below and, for sharps, the one above.
const int8_t LOWER[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
const bool   SHARP[12] = {false, true, false, true, false, false,
                          true, false, true, false, true, false};

constexpr uint16_t PAD_ON = 200;      // below this a pad note counts as silent
constexpr uint16_t PLUCK_ON = 60;
constexpr int32_t PAD_FULL = 36000;   // level shown at full brightness
constexpr int32_t PLUCK_FULL = 7000;
constexpr uint32_t RIPPLE_MS = 1600;  // start-up ripple

// Sound level -> LED brightness, on a log scale (like our ears and eyes):
// "on" is just visible, "full" is full brightness, and every doubling of the
// level in between adds the same amount of light. So a fading pluck glows as
// long as you can hear it, instead of vanishing after its first burst.
uint8_t toTarget(int32_t level, int32_t on, int32_t full) {
  if (level <= on) return 0;
  if (level >= full) return 255;
  float t = logf((float)level / (float)on) / logf((float)full / (float)on);
  return (uint8_t)(255.0f * t);
}

// Smooth the displayed brightness: rise quickly, fall gently.
// `fall` = how many frames a fade roughly takes to settle (higher = slower).
uint8_t smooth(uint8_t shown, uint8_t target, int32_t fall = 6) {
  int32_t s = shown, t = target;
  if (t > s) s += (t - s + 2) / 3;
  else       s -= (s - t + fall - 1) / fall;
  return (uint8_t)s;
}

}  // namespace

Rgb PondLeds::noteColour(uint8_t note) {
  int pc = note % 12;
  Rgb lo = NATURAL[LOWER[pc]];
  if (!SHARP[pc]) return lo;
  Rgb hi = NATURAL[(LOWER[pc] + 1) % 7];
  return Rgb{(uint8_t)((lo.r + hi.r) / 2), (uint8_t)((lo.g + hi.g) / 2), (uint8_t)((lo.b + hi.b) / 2)};
}

// Light on water: every frame (30x a second) the brightness jumps randomly a
// little below its value. The quieter the pluck gets, the deeper the flicker,
// so it sparkles out as it fades.
uint8_t PondLeds::flicker(uint8_t shown) {
  uint32_t depth = 40 + (255u - shown) * 150u / 255u;      // 40..190 of 255
  uint32_t f = 255u - rng_.below(depth + 1);
  return (uint8_t)(shown * f / 255u);
}

void PondLeds::begin(uint32_t seed, uint32_t nowMs) {
  rng_.seed(seed);
  for (auto &s : slots_) s = Slot{-1, 0, false, 0};
  for (auto &t : touchShown_) t = 0;
  startMs_ = nowMs;
}

int PondLeds::findSlot(int src, uint8_t note) const {
  for (int i = 0; i < PADS; i++)
    if (slots_[i].src == src && slots_[i].note == note && !slots_[i].gone) return i;
  return -1;
}

int PondLeds::freeSlot() {
  int free[PADS], n = 0;
  for (int i = 0; i < PADS; i++) if (slots_[i].src < 0) free[n++] = i;
  return n ? free[rng_.below((uint32_t)n)] : -1;
}

void PondLeds::update(const Input &in, uint32_t nowMs, Rgb *out) {
  // 1. Which source is sounding, and how bright should it be?
  int total = PAD_SOURCES + PLUCK_SOURCES;
  uint8_t target[PAD_SOURCES + PLUCK_SOURCES];
  bool on[PAD_SOURCES + PLUCK_SOURCES];
  uint8_t note[PAD_SOURCES + PLUCK_SOURCES];
  for (int s = 0; s < PAD_SOURCES; s++) {
    on[s] = in.padLevel[s] > PAD_ON;
    note[s] = in.padNote[s];
    target[s] = toTarget(in.padLevel[s], PAD_ON, PAD_FULL);
  }
  for (int k = 0; k < PLUCK_SOURCES; k++) {
    int s = PAD_SOURCES + k;
    // Touch plucks are shown on their own pad (step 4 below), not as a slot.
    on[s] = in.pluckLevel[k] > PLUCK_ON && in.pluckTag[k] == 0;
    note[s] = in.pluckNote[k];
    target[s] = toTarget(in.pluckLevel[k], PLUCK_ON, PLUCK_FULL);
  }

  // 2. Slots whose source stopped (or now plays another note) start fading.
  for (auto &sl : slots_) {
    if (sl.src < 0 || sl.gone) continue;
    if (!on[sl.src] || note[sl.src] != sl.note) sl.gone = true;
  }

  // 3. New notes get a random free pad.
  for (int s = 0; s < total; s++) {
    if (!on[s] || findSlot(s, note[s]) >= 0) continue;
    int i = freeSlot();
    if (i < 0) continue;                      // pond is full: not shown
    slots_[i] = Slot{(int8_t)s, note[s], false, 0};
  }

  // 4. Smooth brightness; free fully faded slots.
  for (auto &sl : slots_) {
    if (sl.src < 0) continue;
    uint8_t t = sl.gone ? 0 : target[sl.src];
    sl.shown = smooth(sl.shown, t);
    if (sl.gone && sl.shown == 0) sl.src = -1;
  }

  // 5. Touch plucks: the touched pad glows water blue.
  uint8_t touchT[PADS] = {};
  for (int k = 0; k < PLUCK_SOURCES; k++) {
    uint8_t tag = in.pluckTag[k];
    if (tag == 0 || tag > PADS || in.pluckLevel[k] <= PLUCK_ON) continue;
    uint8_t t = toTarget(in.pluckLevel[k], PLUCK_ON, PLUCK_FULL);
    if (t > touchT[tag - 1]) touchT[tag - 1] = t;
  }
  // (a slower fade, like a ripple settling on the water)
  for (int i = 0; i < PADS; i++) touchShown_[i] = smooth(touchShown_[i], touchT[i], 20);

  // 6. Colours: chord notes in their note colour, plucks water blue with flicker.
  for (int i = 0; i < PADS; i++) {
    Rgb c = colour::BLACK;
    const Slot &sl = slots_[i];
    if (sl.src >= PAD_SOURCES) {
      c = colour::gamma(colour::WATER, flicker(sl.shown), brightness);
    } else if (sl.src >= 0) {
      c = colour::gamma(noteColour(sl.note), sl.shown, brightness);
    }
    if (touchShown_[i]) {
      c = colour::brighter(c, colour::gamma(colour::WATER, flicker(touchShown_[i]), brightness));
    }
    out[i] = c;
  }

  // 7. Start-up: a blue ripple spreading from the middle of the pond.
  uint32_t t = nowMs - startMs_;
  if (t < RIPPLE_MS) {
    float r = 3.2f * (float)t / RIPPLE_MS;            // ring radius in pads
    float fade = 1.0f - (float)t / RIPPLE_MS;
    for (int i = 0; i < PADS; i++) {
      float dx = (float)(i % 4) - 1.5f, dy = (float)(i / 4) - 1.5f;
      float d = sqrtf(dx * dx + dy * dy);
      float k = 1.0f - fabsf(d - r) / 0.9f;
      if (k <= 0) continue;
      uint8_t b = (uint8_t)(255.0f * k * fade);
      out[i] = colour::brighter(out[i], colour::gamma(colour::WATER, b, brightness));
    }
  }
}
