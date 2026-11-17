// Colour - LED colours and helpers shared by the pond and the menu.
#pragma once
#include <stdint.h>

struct Rgb {
  uint8_t r, g, b;
};

namespace colour {

constexpr Rgb BLACK  = {0, 0, 0};
constexpr Rgb RED    = {255, 0, 0};      // menu settings, PLAY when stopped
constexpr Rgb GREEN  = {0, 255, 0};      // PLAY when playing
constexpr Rgb INDIGO = {90, 0, 255};     // scale pads on the ROOT page
constexpr Rgb WATER  = {40, 150, 255};   // plucks, touch, start-up ripple
constexpr Rgb WHITE  = {255, 255, 255};

// Scale by brightness b (0..255) with a gamma curve (b squared), then by the
// master brightness. LEDs look far too bright at low values otherwise, so this
// makes fades look smooth.
inline Rgb gamma(Rgb c, uint8_t b, uint8_t master) {
  uint32_t g = (uint32_t)b * b / 255;
  g = g * master / 255;
  return Rgb{(uint8_t)(c.r * g / 255), (uint8_t)(c.g * g / 255), (uint8_t)(c.b * g / 255)};
}

// Scale linearly (for the menu: "20%" should look like 20%).
inline Rgb linear(Rgb c, uint8_t b, uint8_t master) {
  uint32_t g = (uint32_t)b * master / 255;
  return Rgb{(uint8_t)(c.r * g / 255), (uint8_t)(c.g * g / 255), (uint8_t)(c.b * g / 255)};
}

// Per channel, the brighter of two colours.
inline Rgb brighter(Rgb a, Rgb b) {
  return Rgb{a.r > b.r ? a.r : b.r, a.g > b.g ? a.g : b.g, a.b > b.b ? a.b : b.b};
}

}  // namespace colour
