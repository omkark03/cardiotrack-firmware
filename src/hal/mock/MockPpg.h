#pragma once
#include <Arduino.h>
#include "../IHeartRate.h"

// Fake heart rate: ~75 bpm with a little jitter, always "good".
class MockPpg : public IHeartRate {
public:
  bool begin() override { return true; }
  void poll() override {}
  HrResult finish() override { return HrResult{ 73 + (int)random(0, 5), HrQuality::GOOD }; }
};
