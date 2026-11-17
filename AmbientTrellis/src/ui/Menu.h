// Menu - the SHIFT menu on the 4x4 pads. Runs on core 0.
//
//   SHIFT            open the menu (all 16 parameters light up red)
//   tap a parameter  its value page opens: every possible value glows at 20%,
//                    the current one at full brightness
//   tap a value      it is set, and the menu goes back to the parameter list
//   SHIFT            close the menu; it also closes after 10 s without a touch
//
// Exceptions: PLAY toggles straight away (green = playing, red = stopped);
// ROOT shows note colours for the 12 roots and indigo for the 4 scales;
// LIGHT shows every brightness level so you can see them.
//
// Plain logic (no hardware): it reads pad presses and returns 16 colours.
#pragma once
#include <stdint.h>
#include "../app/Settings.h"
#include "Colour.h"

class Menu {
 public:
  static constexpr uint32_t TIMEOUT_MS = 10000;

  // What a press did, so the app can apply it and show it.
  struct Result {
    bool changed = false;    // a setting changed
    Param param = P_PLAY;    // which one
    bool opened = false;     // the menu or a value page opened (for the display)
    bool closed = false;     // the menu closed
  };

  bool isOpen() const { return state_ != CLOSED; }
  bool onValuePage() const { return state_ == VALUE; }
  Param page() const { return page_; }

  Result shift(uint32_t nowMs);                         // SHIFT pressed
  Result press(uint8_t pad, Settings &s, uint32_t nowMs);  // pad pressed while open
  Result tick(uint32_t nowMs);                          // checks the 10 s timeout

  // Colours for the 16 pads while the menu is open.
  void draw(const Settings &s, Rgb *out) const;

 private:
  enum State : uint8_t { CLOSED, LIST, VALUE };
  State state_ = CLOSED;
  Param page_ = P_PLAY;
  uint32_t lastTouchMs_ = 0;
};
