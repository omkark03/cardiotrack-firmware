#pragma once
#include <stdint.h>

// All pin numbers live here and nowhere else.
// Breadboard and PCB can differ; firmware only uses Pins::*.

namespace Pins {
#if defined(BOARD_BREADBOARD_V1)
  constexpr int ECG_SIG  = 34;  // AD8232 SIG   (ADC1_CH6, input only)
  constexpr int TEMP_ADC = 35;  // NTC divider  (ADC1_CH7, input only)
  constexpr int BAT_ADC  = 36;  // battery divider (ADC1_CH0, input only) - unused unless BATTERY_PRESENT
  constexpr int I2C_SDA  = 21;  // MAX30102 + MPU6050
  constexpr int I2C_SCL  = 22;
  constexpr int IMU_INT  = 27;  // MPU6050 INT -> ext0 wake (RTC-capable)
#elif defined(BOARD_PCB_V1)
  constexpr int ECG_SIG  = 34;
  constexpr int TEMP_ADC = 35;
  constexpr int BAT_ADC  = 36;
  constexpr int I2C_SDA  = 21;
  constexpr int I2C_SCL  = 22;
  constexpr int IMU_INT  = 27;
#else
  #error "Define BOARD_BREADBOARD_V1 or BOARD_PCB_V1 in platformio.ini"
#endif
}

// ---- Compile-time guards ----
// Analog pins must be ADC1 (GPIO32-39); ADC2 is unusable while Wi-Fi is on.
constexpr bool isAdc1(int p) { return p >= 32 && p <= 39; }
// Deep-sleep wake pins must be RTC-capable.
constexpr bool isRtc(int p) {
  return p == 0 || p == 2 || p == 4 || (p >= 12 && p <= 15) ||
         (p >= 25 && p <= 27) || (p >= 32 && p <= 39);
}
static_assert(isAdc1(Pins::ECG_SIG),  "ECG_SIG must be on ADC1 (GPIO32-39)");
static_assert(isAdc1(Pins::TEMP_ADC), "TEMP_ADC must be on ADC1 (GPIO32-39)");
static_assert(isAdc1(Pins::BAT_ADC),  "BAT_ADC must be on ADC1 (GPIO32-39)");
static_assert(isRtc(Pins::IMU_INT),   "IMU_INT must be an RTC-capable GPIO");
