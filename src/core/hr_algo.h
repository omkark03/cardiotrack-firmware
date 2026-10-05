#pragma once
// Portable (no Arduino includes) so it can be tested on the laptop.
#include <stdint.h>

enum class HrQuality : uint8_t { NONE = 0, POOR = 1, GOOD = 2 };

struct HrResult {
  int       bpm;       // 0 when no usable pulse
  HrQuality quality;
};

const char* hrQualityName(HrQuality q);

// ir: raw IR counts sampled at fs Hz. minDc: mean below this = no skin contact.
HrResult computeHr(const int32_t* ir, int n, float fs, int32_t minDc);
