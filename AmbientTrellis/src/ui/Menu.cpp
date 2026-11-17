#include "Menu.h"
#include "PondLeds.h"

namespace {
// Brightness levels (B_ prefix, not to be confused with the menu states)
constexpr uint8_t B_DIM = 51;      // 20%: the rest of a value bar
constexpr uint8_t B_LIST = 140;    // parameter list
constexpr uint8_t B_FULL = 255;

// One colour per row of the menu, all different from the pond's look.
constexpr Rgb ROW[4] = {
  {255, 110,   0},   // row 1: PLAY DENS ROOT DRFT   amber
  {150,   0, 255},   // row 2: VERB LFO BPM BARS     violet
  {255, 255, 255},   // row 3: ATK REL BRIT VOL      white
  {  0, 190, 255},   // row 4: DLY DMOD LITE SLEEP   cyan
};
}  // namespace

Rgb Menu::rowColour(Param p) { return ROW[(p / 4) & 3]; }

Menu::Result Menu::shift(uint32_t nowMs) {
  Result r;
  lastTouchMs_ = nowMs;
  if (state_ == CLOSED) {          // open
    state_ = LIST;
    openedMs_ = nowMs;
    closing_ = false;
    r.opened = true;
  } else if (state_ == VALUE) {    // value page -> back to the list
    state_ = LIST;
    r.back = true;
  } else {                         // list -> close
    state_ = CLOSED;
    closing_ = true;
    closedMs_ = nowMs;
    r.closed = true;
  }
  return r;
}

Menu::Result Menu::tick(uint32_t nowMs) {
  Result r;
  if (state_ != CLOSED && nowMs - lastTouchMs_ >= TIMEOUT_MS) {
    state_ = CLOSED;
    closing_ = true;
    closedMs_ = nowMs;
    r.closed = true;
  }
  return r;
}

Menu::Result Menu::press(uint8_t pad, Settings &s, uint32_t nowMs) {
  Result r;
  if (state_ == CLOSED || pad >= 16) return r;
  lastTouchMs_ = nowMs;

  if (state_ == LIST) {
    Param p = (Param)pad;
    if (p == P_PLAY) {                       // start/stop straight away
      s.playing = !s.playing;
      r.changed = true;
      r.param = P_PLAY;
      return r;
    }
    page_ = p;
    state_ = VALUE;
    r.opened = true;
    r.param = p;
    return r;
  }

  // Value page: set the value and stay on the page.
  if (page_ == P_ROOT) {
    if (pad < 12) s.value[P_ROOT] = (uint8_t)(pad + 1);   // root note
    else s.scale = (uint8_t)(pad - 12);                    // scale
  } else {
    if (pad >= Settings::options(page_)) return r;         // unused pad: ignore
    s.value[page_] = (uint8_t)(pad + 1);
  }
  r.changed = true;
  r.param = page_;
  return r;
}

// The page itself (no animation)
void Menu::drawPage(const Settings &s, Rgb *out) const {
  const uint8_t master = s.ledBrightness();
  for (int i = 0; i < 16; i++) out[i] = colour::BLACK;

  if (state_ == LIST || (state_ == CLOSED && closing_)) {
    for (int i = 0; i < 16; i++) out[i] = colour::linear(rowColour((Param)i), B_LIST, master);
    out[P_PLAY] = colour::linear(s.playing ? colour::GREEN : colour::RED, B_FULL, master);
    return;
  }

  // Value pages
  if (page_ == P_ROOT) {
    uint8_t root = (uint8_t)((s.value[P_ROOT] - 1) % 12);
    for (int i = 0; i < 12; i++) {
      out[i] = colour::linear(PondLeds::noteColour((uint8_t)i), i == root ? B_FULL : B_DIM, master);
    }
    for (int i = 0; i < 4; i++) {
      out[12 + i] = colour::linear(colour::INDIGO, i == s.scale ? B_FULL : B_DIM, master);
    }
    return;
  }

  const Rgb c = rowColour(page_);
  if (page_ == P_LITE) {
    // Every pad shows its own brightness level; the current one is white.
    Settings preview = s;
    for (int i = 0; i < 16; i++) {
      preview.value[P_LITE] = (uint8_t)(i + 1);
      out[i] = colour::linear(i + 1 == s.value[P_LITE] ? colour::WHITE : c, B_FULL,
                              preview.ledBrightness());
    }
    return;
  }

  // A bar: 1..value full, the rest at 20%
  uint8_t n = Settings::options(page_);
  uint8_t cur = s.value[page_];
  for (int i = 0; i < n; i++) out[i] = colour::linear(c, i < cur ? B_FULL : B_DIM, master);
}

bool Menu::draw(const Settings &s, uint32_t nowMs, Rgb *out) const {
  if (state_ == CLOSED && !closing_) return false;

  Rgb page[16];
  drawPage(s, page);

  if (state_ != CLOSED) {
    // Opening sweep: rows appear from top to bottom.
    uint32_t t = nowMs - openedMs_;
    for (int row = 0; row < 4; row++) {
      uint32_t start = row * SWEEP_MS;
      if (t >= start + SWEEP_MS) {
        for (int k = 0; k < 4; k++) out[row * 4 + k] = page[row * 4 + k];
      } else if (t >= start) {
        uint8_t b = (uint8_t)(255u * (t - start) / SWEEP_MS);
        for (int k = 0; k < 4; k++) {
          int i = row * 4 + k;
          // a bright flash at the sweep front, then the page colour
          out[i] = colour::brighter(page[i], colour::linear(colour::WHITE, b, s.ledBrightness()));
        }
      } else {
        for (int k = 0; k < 4; k++) out[row * 4 + k] = colour::BLACK;
      }
    }
    return true;
  }

  // Closing sweep: rows disappear from the bottom up, the pond shows through.
  uint32_t t = nowMs - closedMs_;
  if (t >= 4 * SWEEP_MS) return false;
  for (int row = 0; row < 4; row++) {
    uint32_t gone = (3 - row) * SWEEP_MS;      // bottom row goes first
    if (t < gone) {
      for (int k = 0; k < 4; k++) out[row * 4 + k] = page[row * 4 + k];
    }
  }
  return true;
}
