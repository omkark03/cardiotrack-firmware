#pragma once
// Portable signal-quality helpers (no Arduino includes).
#include <stdint.h>

struct EcgQuality {
  float satPct;      // % of samples railed at either end of the ADC
  float stdCounts;   // standard deviation in ADC counts
  bool  ok;          // false = bad contact / flat line / railed
};
EcgQuality ecgQuality(const int16_t* s, uint32_t n);

enum class Activity : uint8_t { REST = 0, LIGHT = 1, ACTIVE = 2 };
const char* activityName(Activity a);

struct ImuStats {
  float    meanMagG;    // mean |a| in g (should be ~1 when still)
  float    rmsDynG;     // rms of |a| minus its mean, in g
  Activity activity;
};
// magMg: accelerometer magnitude in milli-g
ImuStats imuStats(const int16_t* magMg, uint32_t n);

// Rough CR2450 discharge curve. Returns 0..100, or -1 if mv < 0 (unknown).
int batteryPercent(int mv);
