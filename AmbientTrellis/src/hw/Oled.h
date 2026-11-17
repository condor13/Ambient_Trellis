// Oled - SSD1306 128x64 display over I2C (Wire1, address 0x3C). Core 0.
//
// A full picture is 1 KB, which takes ~25 ms to send at 400 kHz. To keep the
// pads responsive, a picture is sent in its 8 strips ("pages") of 128 bytes,
// one strip per call of sendSome(), with other work in between.
#pragma once
#include <stdint.h>
#include "../ui/Canvas.h"

class Oled {
 public:
  bool begin(uint8_t address = 0x3C);
  bool ok() const { return ok_; }

  // Start sending a new picture (copies it). Returns false if the previous
  // one is still being sent.
  bool startFrame(const Canvas &c);

  // Send one strip of the current picture. Returns true while busy.
  bool sendSome();

  void setContrast(uint8_t c);     // 0..255 (brightness)

 private:
  void command(uint8_t c);
  void command2(uint8_t c, uint8_t arg);

  uint8_t buf_[Canvas::BYTES];
  uint8_t addr_ = 0x3C;
  int8_t page_ = -1;               // strip being sent, -1 = idle
  bool ok_ = false;
};
