#include "SettingsStore.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

void SettingsStore::begin() {
  EEPROM.begin(256);               // reads the flash sector into RAM
}

bool SettingsStore::load(Settings &s) {
  uint8_t buf[Settings::PACKED_SIZE];
  for (int i = 0; i < Settings::PACKED_SIZE; i++) buf[i] = EEPROM.read(i);
  if (!s.unpack(buf)) return false;
  memcpy(last_, buf, sizeof(last_));
  return true;
}

bool SettingsStore::save(const Settings &s) {
  uint8_t buf[Settings::PACKED_SIZE];
  s.pack(buf);
  if (memcmp(buf, last_, sizeof(buf)) == 0) return false;   // nothing new
  for (int i = 0; i < Settings::PACKED_SIZE; i++) EEPROM.write(i, buf[i]);
  EEPROM.commit();                 // the actual flash write (both cores pause briefly)
  memcpy(last_, buf, sizeof(last_));
  return true;
}
