#pragma once
#include <stdint.h>
#include "record.h"

struct CaptureInfo
{
  int batteryMv;   // -1 unknown
  bool timeSynced; // was the clock valid when this burst was captured?
};

// Runs one measurement burst: ECG (250 Hz) + IMU (25 Hz) + HR + temperature + battery,
// fills `r` (waveforms + metadata JSON). Wi-Fi must be OFF while this runs.
// Returns false only if the metadata could not be built.
bool captureBurst(BurstRecord &r, uint32_t seq, uint32_t bootCount, bool motionSinceLast, CaptureInfo &info);
