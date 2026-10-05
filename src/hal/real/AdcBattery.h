#pragma once
#include <Arduino.h>
#include "../IBattery.h"
#include "config.h"

// Cell voltage through a resistor divider into an ADC1 pin.
// PCB note: use a high-value divider (>= 1 MOhm total, to save always-on current)
// with ~100 nF from the ADC pin to GND so the ESP32 ADC can sample it.
//
// NOT yet tested on hardware.
class AdcBattery : public IBattery {
  int pin;
public:
  explicit AdcBattery(int p) : pin(p) {}

  bool begin() override {
    analogReadResolution(12);
    analogSetPinAttenuation(pin, ADC_11db);
    pinMode(pin, INPUT);
    return true;
  }

  int readMv() override {
    uint32_t sum = 0;
    const int N = 16;
    for (int i = 0; i < N; i++) { sum += analogReadMilliVolts(pin); delayMicroseconds(200); }
    return (int)((float)sum / N * Cfg::BAT_DIV_RATIO);
  }
};
