#pragma once
#include <Arduino.h>
#include <math.h>
#include "../IImu.h"

// Fake accelerometer: lying still (gravity on Z) plus noise.
// Build with -DMOCK_WALK=1 to add a 2 Hz, 0.3 g "walking" oscillation
// (useful later for testing the motion-artifact path).
#ifndef MOCK_WALK
#define MOCK_WALK 0
#endif

class MockImu : public IImu {
public:
  bool begin() override { return true; }

  bool readAccel(float& ax, float& ay, float& az) override {
    float n = ((int)random(-100, 101)) / 10000.0f;          // +-0.01 g noise
    float walk = 0;
#if MOCK_WALK
    walk = 0.3f * sinf(2.0f * 3.14159265f * 2.0f * (millis() / 1000.0f));
#endif
    ax = n;
    ay = n * 0.5f;
    az = 1.0f + walk + n;
    return true;
  }

  // Nothing to arm. On the bench, jumper GPIO27 to 3V3 to fake a motion wake.
  bool armMotionWake() override { return true; }
};
