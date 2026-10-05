#pragma once
#include <Arduino.h>
#include "../IEcg.h"

// Real AD8232 on an ADC1 pin.
class AdcEcg : public IEcg {
  int pin;
public:
  explicit AdcEcg(int p) : pin(p) {}

  bool begin() override {
    analogReadResolution(12);
    analogSetPinAttenuation(pin, ADC_11db);   // full ~0-3.3 V range
    pinMode(pin, INPUT);
    return true;
  }

  int16_t readSample() override { return (int16_t)analogRead(pin); }

  // Clone board has no LO+/LO-, so "bad contact" = ADC railed at either end.
  static bool saturated(int16_t v) { return v < 20 || v > 4075; }
};
