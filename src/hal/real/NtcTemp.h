#pragma once
#include <Arduino.h>
#include <math.h>
#include "../ITemp.h"
#include "config.h"

// NTC thermistor, wired 3V3 -> R_FIXED -> node(ADC pin) -> NTC -> GND.
// Beta-equation conversion. Constants (R0, B, offset) are in config.h.
//
// NOT yet tested on hardware. Calibrate TEMP_OFFSET_C against a reference thermometer.
class NtcTemp : public ITemp {
  int pin;
public:
  explicit NtcTemp(int p) : pin(p) {}

  bool begin() override {
    analogReadResolution(12);
    analogSetPinAttenuation(pin, ADC_11db);
    pinMode(pin, INPUT);
    return true;
  }

  bool readC(float& c) override {
    uint32_t sum = 0;
    const int N = 32;
    for (int i = 0; i < N; i++) { sum += analogReadMilliVolts(pin); delayMicroseconds(200); }
    float v = (float)sum / N;
    if (v < 50.0f || v > Cfg::NTC_VCC_MV - 50.0f) return false;     // shorted or open
    float r = Cfg::NTC_R_FIXED * v / (Cfg::NTC_VCC_MV - v);          // NTC resistance
    float invT = 1.0f / (Cfg::NTC_T0_C + 273.15f) + logf(r / Cfg::NTC_R0) / Cfg::NTC_BETA;
    c = 1.0f / invT - 273.15f + Cfg::TEMP_OFFSET_C;
    return true;
  }
};
