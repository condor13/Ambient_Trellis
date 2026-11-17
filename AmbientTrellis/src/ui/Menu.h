// Menu - the SHIFT menu on the 4x4 pads. Runs on core 0.
//
//   SHIFT              open the menu: the 16 parameters light up, one colour per row
//   tap a parameter    its value page opens, in that parameter's colour
//   tap a value        it is set; the page stays open (tap again to change again)
//   SHIFT              on a value page: back to the parameter list
//                      on the parameter list: close the menu
//   (10 s without a touch also closes the menu)
//
// Value pages are bars: pads 1..value at full brightness, the rest at 20%.
// Exceptions: PLAY toggles straight away (green = playing, red = stopped);
// ROOT shows note colours for the 12 roots and indigo for the 4 scales;
// LIGHT shows every brightness level so you can see them.
//
// Opening and closing sweep across the rows, so you can't miss it.
//
// Plain logic (no hardware): it reads pad presses and returns 16 colours.
#pragma once
#include <stdint.h>
#include "../app/Settings.h"
#include "Colour.h"

class Menu {
 public:
  static constexpr uint32_t TIMEOUT_MS = 10000;
  static constexpr uint32_t SWEEP_MS = 60;     // per row when opening/closing

  // What a press did, so the app can apply it and show it.
  struct Result {
    bool changed = false;    // a setting changed
    Param param = P_PLAY;    // which one
    bool opened = false;     // the menu or a value page opened
    bool back = false;       // went back from a value page to the list
    bool closed = false;     // the menu closed
  };

  bool isOpen() const { return state_ != CLOSED; }
  bool onValuePage() const { return state_ == VALUE; }
  Param page() const { return page_; }

  Result shift(uint32_t nowMs);                            // SHIFT pressed
  Result press(uint8_t pad, Settings &s, uint32_t nowMs);  // pad pressed while open
  Result tick(uint32_t nowMs);                             // 10 s timeout

  // Draws the menu over `out` (which holds the pond). Returns false when there
  // is nothing to draw (menu closed and no closing sweep running).
  bool draw(const Settings &s, uint32_t nowMs, Rgb *out) const;

  // The colour of a parameter: one per row.
  static Rgb rowColour(Param p);

 private:
  enum State : uint8_t { CLOSED, LIST, VALUE };
  void drawPage(const Settings &s, Rgb *out) const;

  State state_ = CLOSED;
  Param page_ = P_PLAY;
  uint32_t lastTouchMs_ = 0;
  uint32_t openedMs_ = 0;
  uint32_t closedMs_ = 0;
  bool closing_ = false;
};
