#pragma once
#include <stdint.h>

// Firmware talks only to this interface; mock or real is chosen in factory.cpp.
struct IEcg {
  virtual bool    begin() = 0;
  virtual int16_t readSample() = 0;   // raw 12-bit, 0..4095
  virtual ~IEcg() {}
};
