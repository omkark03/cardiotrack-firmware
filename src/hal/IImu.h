#pragma once

struct IImu {
  // Normal accel-only sampling mode (used during the burst).
  virtual bool begin() = 0;
  virtual bool readAccel(float& ax, float& ay, float& az) = 0;   // in g
  // Low-power accel-only + motion interrupt, used just before deep sleep.
  // Must also clear any pending interrupt so the wake pin is low.
  virtual bool armMotionWake() = 0;
  virtual ~IImu() {}
};
