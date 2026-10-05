#pragma once
#include <Arduino.h>
#include "../ITemp.h"

// Fake skin temperature: 36.6 C +- 0.05
class MockTemp : public ITemp {
public:
  bool begin() override { return true; }
  bool readC(float& c) override { c = 36.6f + ((int)random(-5, 6)) / 100.0f; return true; }
};
