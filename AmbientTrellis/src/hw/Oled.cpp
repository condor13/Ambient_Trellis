#include "Oled.h"
#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// Every I2C message to the SSD1306 starts with a control byte:
// 0x00 = commands follow, 0x40 = picture data follows.
static constexpr uint8_t CTRL_CMD = 0x00;
static constexpr uint8_t CTRL_DATA = 0x40;
static constexpr int CHUNK = 32;   // data bytes per I2C message (fits every Wire buffer)

void Oled::command(uint8_t c) {
  Wire.beginTransmission(addr_);
  Wire.write(CTRL_CMD);
  Wire.write(c);
  Wire.endTransmission();
}

void Oled::command2(uint8_t c, uint8_t arg) {
  Wire.beginTransmission(addr_);
  Wire.write(CTRL_CMD);
  Wire.write(c);
  Wire.write(arg);
  Wire.endTransmission();
}

bool Oled::begin(uint8_t address) {
  addr_ = address;
  Wire.beginTransmission(addr_);
  ok_ = (Wire.endTransmission() == 0);      // is anyone there?
  if (!ok_) return false;

  // Standard start-up sequence for a 128x64 SSD1306 with its own charge pump
  command(0xAE);           // display off
  command2(0xD5, 0x80);    // clock
  command2(0xA8, 0x3F);    // 64 rows
  command2(0xD3, 0x00);    // no vertical offset
  command(0x40);           // start at row 0
  command2(0x8D, 0x14);    // charge pump on
  command2(0x20, 0x00);    // horizontal addressing: data fills columns, then the next page
  command(0xA1);           // mirror columns (so x=0 is on the left)
  command(0xC8);           // mirror rows (so y=0 is at the top)
  command2(0xDA, 0x12);    // row wiring for 64-row panels
  command2(0x81, 0x80);    // contrast
  command2(0xD9, 0xF1);    // pre-charge
  command2(0xDB, 0x40);    // VCOMH level
  command(0xA4);           // show the RAM contents
  command(0xA6);           // normal (not inverted)
  command(0x2E);           // no scrolling
  command(0xAF);           // display on

  memset(buf_, 0, sizeof(buf_));
  page_ = 0;
  while (sendSome()) {}    // start with a black screen
  return true;
}

void Oled::setContrast(uint8_t c) {
  if (ok_) command2(0x81, c);
}

bool Oled::startFrame(const Canvas &c) {
  if (!ok_ || page_ >= 0) return false;
  memcpy(buf_, c.data(), sizeof(buf_));
  page_ = 0;
  return true;
}

bool Oled::sendSome() {
  if (!ok_ || page_ < 0) return false;

  // Where to write: all 128 columns of this one page
  Wire.beginTransmission(addr_);
  Wire.write(CTRL_CMD);
  Wire.write(0x21); Wire.write(0); Wire.write(127);           // columns
  Wire.write(0x22); Wire.write((uint8_t)page_); Wire.write((uint8_t)page_);   // page
  Wire.endTransmission();

  const uint8_t *p = buf_ + page_ * 128;
  for (int i = 0; i < 128; i += CHUNK) {
    Wire.beginTransmission(addr_);
    Wire.write(CTRL_DATA);
    Wire.write(p + i, CHUNK);
    Wire.endTransmission();
  }

  if (++page_ >= 8) page_ = -1;
  return page_ >= 0;
}
