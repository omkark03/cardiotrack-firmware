#include <Arduino.h>
#include "board_pins.h"
#include "config.h"

#ifdef APP_ECG_STREAM
// ---------------------------------------------------------------------------
// ECG debug mode: stream raw ECG at 250 Hz to Serial Plotter (115200 baud).
// Use env stream_real to look at the real AD8232, stream_mock for the fake one.
// ---------------------------------------------------------------------------
#include "hal/factory.h"

static uint32_t nextTick;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("# ECG stream | ECG pin=%d | ", Pins::ECG_SIG);
#ifdef USE_MOCK_SENSORS
  Serial.println("MOCK");
#else
  Serial.println("REAL");
#endif
  delay(500);
  ecg().begin();
  nextTick = micros();
}

void loop() {
  nextTick += 1000000UL / Cfg::ECG_SAMPLE_HZ;
  Serial.println(ecg().readSample());
  while ((int32_t)(micros() - nextTick) < 0) {}
}

#else
// ---------------------------------------------------------------------------
// Normal firmware: wake -> measure -> upload -> sleep (see app/app.cpp)
// ---------------------------------------------------------------------------
#include "app/app.h"

void setup() { App::setup(); }
void loop()  { App::loop(); }

#endif
