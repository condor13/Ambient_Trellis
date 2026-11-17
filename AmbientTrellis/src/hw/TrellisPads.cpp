#include "TrellisPads.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoTrellis.h>
#include "../util/PadMap.h"

// The library calls a plain function for every key event, so the presses go
// into a small queue here and TrellisPads hands them out one by one.
static Adafruit_NeoTrellis trellis(NEO_TRELLIS_ADDR, &Wire1);
static uint8_t  pressQ[16];
static uint8_t  qHead = 0, qTail = 0;
static uint16_t downMask = 0;

static TrellisCallback onKey(keyEvent evt) {
  if (evt.bit.NUM >= 16) return 0;
  uint8_t k = padmap::toLogical(evt.bit.NUM);   // turned grid -> logical pad
  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    downMask |= (uint16_t)(1u << k);
    if ((uint8_t)(qHead - qTail) < sizeof(pressQ)) pressQ[qHead++ & 15] = k;
  } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
    downMask &= (uint16_t)~(1u << k);
  }
  return 0;
}

bool TrellisPads::begin() {
  ok_ = trellis.begin();
  if (!ok_) return false;

  for (int i = 0; i < 16; i++) {
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING);
    trellis.registerCallback(i, onKey);
  }
  trellis.pixels.setBrightness(255);   // brightness is handled in PondLeds
  for (int i = 0; i < 16; i++) trellis.pixels.setPixelColor(i, 0, 0, 0);
  trellis.pixels.show();
  return true;
}

void TrellisPads::poll() {
  if (ok_) trellis.read();
}

bool TrellisPads::nextPress(uint8_t &pad) {
  if (qHead == qTail) return false;
  pad = pressQ[qTail++ & 15];
  return true;
}

bool TrellisPads::isDown(uint8_t pad) const { return (downMask >> pad) & 1; }

void TrellisPads::show(const Rgb *c) {
  if (!ok_) return;
  for (int i = 0; i < 16; i++) {
    const Rgb &col = c[padmap::toLogical((uint8_t)i)];   // physical LED i shows its logical pad
    trellis.pixels.setPixelColor(i, col.r, col.g, col.b);
  }
  trellis.pixels.show();
}
