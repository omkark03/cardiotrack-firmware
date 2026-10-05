#pragma once
#include <stdint.h>

namespace SleepMgr {
  // Deep sleep for `seconds`. If armMotion, the MPU6050 interrupt can wake us earlier.
  // Does not return on hardware. With -DBENCH_MODE it just waits and returns.
  void sleepFor(uint32_t seconds, bool armMotion);
}
