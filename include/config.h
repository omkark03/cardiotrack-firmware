#pragma once
#include <stdint.h>

// Every tunable number lives here. Override timing from platformio.ini with
//   -DCFG_BURST_SEC=10 -DCFG_CYCLE_SEC=30   (bench tests)

#ifndef CFG_BURST_SEC
#define CFG_BURST_SEC 60
#endif
#ifndef CFG_CYCLE_SEC
#define CFG_CYCLE_SEC 300
#endif

#define FW_VERSION "0.1.0"
#define DEVICE_ID "chest_hub_001"

namespace Cfg
{

    // ---- Timing -------------------------------------------------------------
    constexpr uint32_t ECG_SAMPLE_HZ = 250;
    constexpr uint32_t BURST_SEC = CFG_BURST_SEC;
    constexpr uint32_t CYCLE_SEC = CFG_CYCLE_SEC; // wake-to-wake period
    constexpr uint32_t MIN_SLEEP_SEC = 5;

    // ---- Buffers (sized from the burst length) -------------------------------
    constexpr uint32_t ECG_SAMPLES = ECG_SAMPLE_HZ * BURST_SEC; // int16 each
    constexpr uint32_t IMU_HZ = 25;                             // accel magnitude, int16 mg
    constexpr uint32_t IMU_DECIM = ECG_SAMPLE_HZ / IMU_HZ;      // read IMU every N ECG ticks
    constexpr uint32_t IMU_SAMPLES = IMU_HZ * BURST_SEC;
    constexpr uint32_t PPG_HZ = 25;                           // 100 Hz sensor, 4x averaged
    constexpr uint32_t PPG_POLL_TICKS = 25;                   // drain MAX30102 FIFO every 100 ms
    constexpr uint32_t PPG_SAMPLES = PPG_HZ * BURST_SEC + 64; // int32 each (internal to driver)
    constexpr uint32_t META_MAX = 768;                        // JSON metadata buffer

    static_assert(ECG_SAMPLE_HZ % IMU_HZ == 0, "IMU rate must divide ECG rate");
    static_assert(CYCLE_SEC >= BURST_SEC + 15, "cycle must leave time for upload");

    // ---- Signal-quality thresholds -------------------------------------------
    constexpr int16_t ECG_SAT_LOW = 20;     // ADC at/below -> railed
    constexpr int16_t ECG_SAT_HIGH = 4075;  // ADC at/above -> railed
    constexpr float ECG_SAT_MAX_PCT = 5.0f; // >5% railed samples -> bad contact
    constexpr float ECG_FLAT_STD = 3.0f;    // std-dev (ADC counts) below this -> flat line

    constexpr float TEMP_MIN_C = 25.0f; // outside this range -> sensor fault / not on skin
    constexpr float TEMP_MAX_C = 42.0f;

    constexpr int32_t PPG_MIN_DC = 10000; // mean IR (18-bit counts) below this -> no skin contact

    constexpr float ACT_REST_G = 0.03f;  // rms of dynamic accel (g): below = resting
    constexpr float ACT_LIGHT_G = 0.15f; //                          below = light, else active

    // ---- MAX30102 (IR only) ---------------------------------------------------
    constexpr uint8_t PPG_LED_PA = 0x24; // IR LED current register (0x24 ~ 7 mA); tune on the chest
    constexpr uint32_t PPG_SKIP_SEC = 2; // ignore first seconds after LED turn-on

    // ---- MPU6050 wake-on-motion ----------------------------------------------
    constexpr uint8_t IMU_MOTION_THR = 20; // 1 LSB = 2 mg -> 40 mg
    constexpr uint8_t IMU_LP_WAKE = 1;     // 0=1.25 Hz 1=5 Hz 2=20 Hz 3=40 Hz

    // ---- Thermistor (3V3 -> R_FIXED -> node(ADC) -> NTC -> GND) ---------------
    constexpr float NTC_R_FIXED = 10000.0f;
    constexpr float NTC_R0 = 10000.0f; // NTC resistance at T0
    constexpr float NTC_T0_C = 25.0f;
    constexpr float NTC_BETA = 3950.0f; // CHECK against your part's datasheet
    constexpr float NTC_VCC_MV = 3300.0f;
    constexpr float TEMP_OFFSET_C = 0.0f; // calibrate against a reference thermometer

    // ---- Battery (CR2450 through a divider) ----------------------------------
    constexpr float BAT_DIV_RATIO = 2.0f; // Vcell = Vadc * ratio (2 equal resistors)
    constexpr int BAT_LOW_MV = 2500;
    constexpr uint32_t BAT_LOW_CYCLE_FACTOR = 3; // stretch the cycle x3 when battery is low

    // ---- Outage queue --------------------------------------------------------
    constexpr uint32_t QUEUE_MAX_RECORDS = 40;    // ~33 KB each; oldest dropped when full
    constexpr uint32_t QUEUE_FREE_RESERVE = 8192; // keep this much flash free
    constexpr uint32_t MAX_FLUSH_PER_CYCLE = 4;   // backlog drains over several cycles

    // ---- Wi-Fi / network -----------------------------------------------------
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
    constexpr uint32_t HTTP_TIMEOUT_MS = 8000;
    constexpr uint32_t NTP_TIMEOUT_MS = 10000;         // lwIP waits a random 0-5 s before its first request
    constexpr uint32_t MIN_VALID_EPOCH = 1700000000UL; // clock below this = never synced
    constexpr uint32_t NTP_RESYNC_CYCLES = 12;         // re-sync the clock every N bursts (RC oscillator drifts)
}

// Lower TX power = lower current spike (helps the supercap). Tune on hardware.
#define CFG_WIFI_TX_POWER WIFI_POWER_15dBm