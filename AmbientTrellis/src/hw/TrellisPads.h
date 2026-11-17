// TrellisPads - the Adafruit NeoTrellis 4x4 (keys + NeoPixels) over I2C.
//
// A thin hardware layer: read key presses, set 16 colours. All the logic of
// what to show lives in src/ui/PondLeds. Runs on core 0.
//
// Pad numbers are logical pads 0..15 (row by row from the top left). The grid
// can be turned on the board with PAD_ROTATION in Config.h (see util/PadMap.h).
#pragma once
#include <stdint.h>
#include "../ui/PondLeds.h"

class TrellisPads {
 public:
  // The I2C bus (Wire1 on GP26/27) must already be set up.
  bool begin();
  bool ok() const { return ok_; }

  // Read the keys (call ~every 20 ms). Presses are queued for nextPress().
  void poll();
  bool nextPress(uint8_t &pad);
  bool isDown(uint8_t pad) const;

  // Send 16 colours to the pads.
  void show(const Rgb *colours);

 private:
  bool ok_ = false;
};
