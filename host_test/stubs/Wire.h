#pragma once
#include "Arduino.h"

// I2C with nothing attached: every transfer is NACKed. Used to check that the real
// sensor drivers fail gracefully when a sensor is missing / unwired.
class TwoWire {
public:
  bool begin(int, int) { return true; }
  void setClock(uint32_t) {}
  void setTimeOut(uint16_t) {}
  void beginTransmission(uint8_t) {}
  size_t write(uint8_t) { return 1; }
  uint8_t endTransmission(bool = true) { return 2; }                  // 2 = NACK on address
  size_t requestFrom(uint16_t, size_t, bool = true) { return 0; }
  int read() { return 0; }
};
inline TwoWire Wire;
