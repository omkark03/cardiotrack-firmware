#pragma once
#include <stdint.h>
#include "config.h"

// One burst, ready to send or to park in flash. A single static instance is
// reused for the fresh burst and for queued records (never two at once).
struct BurstRecord {
  char     meta[Cfg::META_MAX];
  uint32_t metaLen;
  int16_t  ecg[Cfg::ECG_SAMPLES];
  uint32_t ecgN;
  int16_t  imu[Cfg::IMU_SAMPLES];     // accel magnitude, milli-g, 25 Hz
  uint32_t imuN;
};

inline BurstRecord& record() {
  static BurstRecord r;
  return r;
}
