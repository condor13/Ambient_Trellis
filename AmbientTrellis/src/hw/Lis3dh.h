// Lis3dh - the LIS3DH accelerometer on the NeoTrellis I2C bus (Wire1). Core 0.
//
// A minimal driver: find the chip (address 0x18 or 0x19), set it to 100
// readings per second at +-2 g, and read x, y, z in milli-g (1000 = gravity).
#pragma once
#include <stdint.h>

class Lis3dh {
 public:
  bool begin();                       // tries 0x18, then 0x19
  bool ok() const { return addr_ != 0; }
  uint8_t address() const { return addr_; }
  bool read(int32_t &x, int32_t &y, int32_t &z);

 private:
  bool writeReg(uint8_t reg, uint8_t val);
  bool readRegs(uint8_t reg, uint8_t *buf, int n);
  uint8_t addr_ = 0;
};
