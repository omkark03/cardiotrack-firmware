#pragma once
// Portable (no Arduino includes): builds the metadata JSON and the multipart body.
// Tested on the laptop and the same body is fed to the fake backend.
#include <stddef.h>
#include <stdint.h>

struct Meta
{
  const char *deviceId;
  const char *fwVersion;
  uint32_t seq;       // increments every burst (survives deep sleep)
  uint32_t bootCount; // increments on power-up / reset
  int64_t epoch;      // capture start, UTC seconds. Seconds since power-up if not synced
  bool timeSynced;

  // ECG
  uint32_t ecgSamples;
  uint32_t ecgRateHz;
  uint32_t burstSec;
  float ecgSatPct;
  float ecgStd;
  bool ecgOk;

  // IMU
  uint32_t imuSamples;
  uint32_t imuRateHz;
  bool imuOk;
  float meanMagG;
  float rmsDynG;
  float ax, ay, az;     // mean accel, g
  const char *activity; // "resting" | "light" | "active"
  bool motionSinceLast; // motion wake happened since the previous burst

  // Heart rate
  int hrBpm;             // 0 = unknown
  const char *hrQuality; // "good" | "poor" | "none"
  bool hrOk;             // sensor initialised

  // Temperature
  float skinTempC; // NAN = no reading
  bool tempOk;     // reading present and plausible

  // Battery
  int batteryMv;  // -1 unknown
  int batteryPct; // -1 unknown
};

// Returns the JSON length (excluding NUL), or 0 if it did not fit.
size_t buildMetaJson(char *out, size_t cap, const Meta &m);

// The clock got synced AFTER the burst was captured: rewrite timestamp/epoch/time_synced inside an
// already built meta JSON (epoch = old capture epoch + clock jump) and add "time_estimated":true.
// Returns the new length, or 0 (meta untouched) if the JSON does not have the expected shape / does not fit.
size_t patchMetaTime(char *meta, size_t len, size_t cap, int64_t newEpoch);

// Total size of the multipart body for the given parts.
size_t multipartSize(const char *boundary, size_t metaLen, size_t ecgBytes, size_t imuBytes);

// Builds: form field "meta" (JSON), file "ecg" (int16 LE), file "imu" (int16 LE).
// Returns bytes written, or 0 if cap is too small.
size_t buildMultipart(uint8_t *out, size_t cap, const char *boundary,
                      const char *meta, size_t metaLen,
                      const int16_t *ecg, size_t ecgN,
                      const int16_t *imu, size_t imuN);
