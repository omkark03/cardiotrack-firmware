#pragma once
#include <Arduino.h>
#include <math.h>
#include "../IEcg.h"
#include "config.h"

// Synthetic ECG: P, Q, R, S, T waves as Gaussians, ~75 bpm, small noise.
class MockEcg : public IEcg {
  uint32_t n = 0;
  float    bpm = 75.0f;

  static float gauss(float t, float mu, float sigma, float amp) {
    float d = (t - mu) / sigma;
    return amp * expf(-0.5f * d * d);
  }

public:
  bool begin() override { n = 0; return true; }

  int16_t readSample() override {
    float period = 60.0f / bpm;                              // seconds per beat
    float t = fmodf((float)n / Cfg::ECG_SAMPLE_HZ, period) / period;  // 0..1 within beat
    n++;

    float v = 0;
    v += gauss(t, 0.20f, 0.025f,  0.12f);   // P
    v += gauss(t, 0.36f, 0.010f, -0.12f);   // Q
    v += gauss(t, 0.40f, 0.012f,  1.00f);   // R
    v += gauss(t, 0.44f, 0.010f, -0.25f);   // S
    v += gauss(t, 0.65f, 0.045f,  0.30f);   // T
    v += ((int)random(-100, 101)) / 3000.0f; // noise

    int adc = 1800 + (int)(v * 1200.0f);     // baseline ~1800, R peak ~3000
    if (adc < 0) adc = 0;
    if (adc > 4095) adc = 4095;
    return (int16_t)adc;
  }
};
