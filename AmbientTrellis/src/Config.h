// Ambient Trellis - hardware pins and audio constants.
#pragma once
#include <stdint.h>

namespace cfg {

// I2S DAC (PCM5102A). LRCK is always BCK + 1, so GP19.
constexpr int PIN_I2S_BCK = 18;
constexpr int PIN_I2S_DIN = 20;

// I2C1 (Wire1): NeoTrellis (0x2E), LIS3DH (0x18/0x19, later)
constexpr int PIN_SDA = 26;
constexpr int PIN_SCL = 27;
constexpr uint32_t I2C_HZ = 400000;

// I2C0 (Wire): the OLED on its own bus
constexpr int PIN_OLED_SDA = 16;
constexpr int PIN_OLED_SCL = 17;
constexpr uint32_t OLED_I2C_HZ = 400000;
constexpr uint8_t OLED_ADDR = 0x3C;

// Orientation
constexpr int  PAD_ROTATION = 90;            // pads: 0, 90, 180 or 270 degrees clockwise
constexpr bool OLED_FLIP = true;             // display upside down (rotated 180 degrees)
constexpr bool OLED_GRID_FOLLOWS_PADS = true;  // menu labels on the display sit like the pads

// SHIFT key, wired to GND, internal pull-up
constexpr int PIN_SHIFT = 6;

// Audio
constexpr uint32_t SAMPLE_RATE = 32000;
constexpr int      BLOCK       = 32;    // samples per control block = 1 ms
constexpr int      BLOCK_SHIFT = 5;     // log2(BLOCK)
constexpr int      NUM_VOICES  = 16;    // pad voices

}  // namespace cfg
