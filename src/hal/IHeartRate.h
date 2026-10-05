#pragma once
#include "../core/hr_algo.h"

// Heart rate from the chest-wing PPG sensor.
// Usage per burst: begin() -> poll() every ~100 ms -> finish().
struct IHeartRate {
  virtual bool     begin() = 0;    // configure + start sampling
  virtual void     poll() = 0;     // drain FIFO into an internal buffer
  virtual HrResult finish() = 0;   // compute bpm, put sensor in shutdown
  virtual ~IHeartRate() {}
};
