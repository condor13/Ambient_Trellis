// Screen - what the 128x64 OLED shows. Runs on core 0.
//
// Play mode: a koi pond. Three koi swim slow curves (faster when the LFO is
// faster), lily pads float on top, and ripples spread when a chord changes,
// a pluck sounds or a pad is touched. When something changes, a short text
// box appears over the pond for 2 seconds.
//
// Menu: a 4x4 grid of labels, laid out like the pads. A value page shows the
// parameter's name, its value and a bar like the one on the pads.
//
// Plain logic: it draws into a Canvas, so it can be previewed on a computer.
#pragma once
#include <stdint.h>
#include "../app/Settings.h"
#include "../util/Rng.h"
#include "Canvas.h"
#include "Menu.h"

class Screen {
 public:
  void begin(uint32_t seed, uint32_t nowMs);

  // Events from the music
  void rippleRandom(float maxRadius);       // chord change, generative pluck
  void ripplePad(uint8_t pad);              // a touched pad, at its place on the pond

  // A text box over the pond (title small, value large; value may be null).
  void overlay(const char *title, const char *value, uint32_t nowMs, uint32_t ms = 2000);

  // Draw the next frame.
  void draw(Canvas &c, const Settings &s, const Menu &m, uint32_t nowMs);

 private:
  struct Fish {
    float x, y, heading, turn, phase;
    uint8_t spots;       // koi pattern: which body spots are dark
  };
  struct Ripple {
    float x, y, r, maxR;
    bool alive;
  };
  static constexpr int FISH = 3;
  static constexpr int RIPPLES = 8;

  void updatePond(float dt, float speed);
  void drawPond(Canvas &c) const;
  void drawFish(Canvas &c, const Fish &f) const;
  void drawMenuList(Canvas &c, const Settings &s) const;
  void drawValuePage(Canvas &c, const Settings &s, Param p) const;
  void drawOverlay(Canvas &c) const;
  void addRipple(float x, float y, float maxR);

  Fish fish_[FISH];
  Ripple ripples_[RIPPLES];
  Rng rng_;
  uint32_t lastMs_ = 0;

  char ovTitle_[24] = "";
  char ovValue_[24] = "";
  uint32_t ovUntilMs_ = 0;
};
