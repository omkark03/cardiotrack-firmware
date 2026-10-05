#include "sleep_mgr.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "board_pins.h"
#include "log.h"
#include "../hal/factory.h"

namespace SleepMgr
{

  void sleepFor(uint32_t seconds, bool armMotion)
  {
    if (armMotion)
    {
      if (!imu().armMotionWake())
        LOG("sleep: IMU wake-on-motion setup failed");
    }

#ifdef BENCH_MODE
    LOG("bench: pretending to sleep %lu s (motion %s)", (unsigned long)seconds, armMotion ? "armed" : "off");
    delay(seconds * 1000UL);
    return;
#else
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    if (armMotion)
    {
      esp_sleep_enable_ext0_wakeup((gpio_num_t)Pins::IMU_INT, 1); // INT is active high
      rtc_gpio_pullup_dis((gpio_num_t)Pins::IMU_INT);
      rtc_gpio_pulldown_en((gpio_num_t)Pins::IMU_INT); // no spurious wake if the pin floats
    }
    // No else: wake-up sources are reset on every boot, so "motion off" simply means ext0 is never enabled.
    // (esp_sleep_disable_wakeup_source(EXT0) is not supported by IDF 4.4 and only prints an error.)
    LOG("sleep: %lu s (motion %s)", (unsigned long)seconds, armMotion ? "armed" : "off");
    Serial.flush();
    esp_deep_sleep_start();
#endif
  }

} // namespace SleepMgr
