#include "Lis3dh.h"
#include <Arduino.h>
#include <Wire.h>

// Registers
static constexpr uint8_t WHO_AM_I = 0x0F;    // reads 0x33 on a LIS3DH
static constexpr uint8_t CTRL_REG1 = 0x20;
static constexpr uint8_t CTRL_REG4 = 0x23;
static constexpr uint8_t OUT_X_L = 0x28;
static constexpr uint8_t AUTO_INC = 0x80;    // read several registers in a row

bool Lis3dh::writeReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(addr_);
  Wire1.write(reg);
  Wire1.write(val);
  return Wire1.endTransmission() == 0;
}

bool Lis3dh::readRegs(uint8_t reg, uint8_t *buf, int n) {
  Wire1.beginTransmission(addr_);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(addr_, (size_t)n) != (size_t)n) return false;
  for (int i = 0; i < n; i++) buf[i] = (uint8_t)Wire1.read();
  return true;
}

bool Lis3dh::begin() {
  const uint8_t candidates[2] = {0x18, 0x19};
  for (uint8_t a : candidates) {
    addr_ = a;
    uint8_t id = 0;
    if (readRegs(WHO_AM_I, &id, 1) && id == 0x33) {
      writeReg(CTRL_REG1, 0x57);   // 100 Hz, normal mode, x/y/z on
      writeReg(CTRL_REG4, 0x08);   // +-2 g, high resolution (12 bit)
      return true;
    }
  }
  addr_ = 0;
  return false;
}

bool Lis3dh::read(int32_t &x, int32_t &y, int32_t &z) {
  if (!addr_) return false;
  uint8_t b[6];
  if (!readRegs(OUT_X_L | AUTO_INC, b, 6)) return false;
  // 12-bit values, left-aligned in 16 bits. At +-2 g high resolution,
  // one step is 1 milli-g, so >> 4 gives milli-g directly.
  x = (int16_t)(b[0] | (b[1] << 8)) >> 4;
  y = (int16_t)(b[2] | (b[3] << 8)) >> 4;
  z = (int16_t)(b[4] | (b[5] << 8)) >> 4;
  return true;
}
