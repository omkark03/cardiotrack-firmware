#include "factory.h"
#include "board_pins.h"

// -DUSE_MOCK_SENSORS = all four mock. Or pick individually with
// -DUSE_MOCK_ECG / _PPG / _IMU / _TEMP (handy while bringing up hardware).
#ifdef USE_MOCK_SENSORS
  #ifndef USE_MOCK_ECG
    #define USE_MOCK_ECG
  #endif
  #ifndef USE_MOCK_PPG
    #define USE_MOCK_PPG
  #endif
  #ifndef USE_MOCK_IMU
    #define USE_MOCK_IMU
  #endif
  #ifndef USE_MOCK_TEMP
    #define USE_MOCK_TEMP
  #endif
#endif

// ---- ECG ----
#ifdef USE_MOCK_ECG
  #include "mock/MockEcg.h"
  IEcg& ecg() { static MockEcg s; return s; }
#else
  #include "real/AdcEcg.h"
  IEcg& ecg() { static AdcEcg s(Pins::ECG_SIG); return s; }
#endif

// ---- Heart rate ----
#ifdef USE_MOCK_PPG
  #include "mock/MockPpg.h"
  IHeartRate& ppg() { static MockPpg s; return s; }
#else
  #include "real/Max30102.h"
  IHeartRate& ppg() { static Max30102 s; return s; }
#endif

// ---- IMU ----
#ifdef USE_MOCK_IMU
  #include "mock/MockImu.h"
  IImu& imu() { static MockImu s; return s; }
#else
  #include "real/Mpu6050.h"
  IImu& imu() { static Mpu6050 s; return s; }
#endif

// ---- Skin temperature ----
#ifdef USE_MOCK_TEMP
  #include "mock/MockTemp.h"
  ITemp& tempSensor() { static MockTemp s; return s; }
#else
  #include "real/NtcTemp.h"
  ITemp& tempSensor() { static NtcTemp s(Pins::TEMP_ADC); return s; }
#endif

// ---- Battery: only the PCB has the divider ----
#ifdef BATTERY_PRESENT
  #include "real/AdcBattery.h"
  IBattery& battery() { static AdcBattery s(Pins::BAT_ADC); return s; }
#else
  #include "mock/NoBattery.h"
  IBattery& battery() { static NoBattery s; return s; }
#endif
