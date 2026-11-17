#include "Menu.h"
#include "PondLeds.h"

namespace {
// (B_ prefix: brightness levels, not to be confused with the menu states)
constexpr uint8_t B_DIM = 80;      // ~30%: the "other values" on a value page
constexpr uint8_t B_LIST = 110;    // parameter list (~45%)
constexpr uint8_t B_FULL = 255;
}  // namespace

Menu::Result Menu::shift(uint32_t nowMs) {
  Result r;
  lastTouchMs_ = nowMs;
  if (state_ == CLOSED) {
    state_ = LIST;
    r.opened = true;
  } else {
    state_ = CLOSED;
    r.closed = true;
  }
  return r;
}

Menu::Result Menu::tick(uint32_t nowMs) {
  Result r;
  if (state_ != CLOSED && nowMs - lastTouchMs_ >= TIMEOUT_MS) {
    state_ = CLOSED;
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

  // Value page
  if (page_ == P_ROOT) {
    if (pad < 12) s.value[P_ROOT] = (uint8_t)(pad + 1);   // root note
    else s.scale = (uint8_t)(pad - 12);                    // scale
  } else {
    if (pad >= Settings::options(page_)) return r;         // unused pad: ignore
    s.value[page_] = (uint8_t)(pad + 1);
  }
  r.changed = true;
  r.param = page_;
  state_ = LIST;                                           // back to the list
  return r;
}

void Menu::draw(const Settings &s, Rgb *out) const {
  const uint8_t master = s.ledBrightness();
  for (int i = 0; i < 16; i++) out[i] = colour::BLACK;
  if (state_ == CLOSED) return;

  if (state_ == LIST) {
    for (int i = 0; i < 16; i++) out[i] = colour::linear(colour::RED, B_LIST, master);
    out[P_PLAY] = colour::linear(s.playing ? colour::GREEN : colour::RED, B_FULL, master);
    return;
  }

  // Value pages
  if (page_ == P_ROOT) {
    uint8_t root = (uint8_t)((s.value[P_ROOT] - 1) % 12);
    for (int i = 0; i < 12; i++) {
      Rgb c = PondLeds::noteColour((uint8_t)i);
      out[i] = colour::linear(c, i == root ? B_FULL : B_DIM, master);
    }
    for (int i = 0; i < 4; i++) {
      out[12 + i] = colour::linear(colour::INDIGO, i == s.scale ? B_FULL : B_DIM, master);
    }
    return;
  }

  if (page_ == P_LITE) {
    // Every pad shows its own brightness level; the current one is white.
    Settings preview = s;
    for (int i = 0; i < 16; i++) {
      preview.value[P_LITE] = (uint8_t)(i + 1);
      Rgb c = (i + 1 == s.value[P_LITE]) ? colour::WHITE : colour::RED;
      out[i] = colour::linear(c, B_FULL, preview.ledBrightness());
    }
    return;
  }

  uint8_t n = Settings::options(page_);
  uint8_t cur = s.value[page_];
  for (int i = 0; i < n; i++) {
    out[i] = colour::linear(colour::RED, i + 1 == cur ? B_FULL : B_DIM, master);
  }
}
