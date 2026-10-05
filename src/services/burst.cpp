#include "burst.h"
#include <Arduino.h>
#include <math.h>
#include <time.h>
#include "config.h"
#include "log.h"
#include "../hal/factory.h"
#include "../core/payload.h"
#include "../core/quality.h"
#include "../core/hr_algo.h"

bool captureBurst(BurstRecord &r, uint32_t seq, uint32_t bootCount, bool motionSinceLast, CaptureInfo &info)
{
  Meta m{};
  m.deviceId = DEVICE_ID;
  m.fwVersion = FW_VERSION;
  m.seq = seq;
  m.bootCount = bootCount;
  m.epoch = (int64_t)time(nullptr);
  m.timeSynced = m.epoch >= (int64_t)Cfg::MIN_VALID_EPOCH;
  m.motionSinceLast = motionSinceLast;
  info.timeSynced = m.timeSynced;

  bool ecgInit = ecg().begin();
  bool ppgInit = ppg().begin();
  bool imuInit = imu().begin();
  bool tempInit = tempSensor().begin();
  battery().begin();
  LOG("burst %lu: init ecg=%d ppg=%d imu=%d temp=%d", (unsigned long)seq, ecgInit, ppgInit, imuInit, tempInit);

  info.batteryMv = battery().readMv(); // read before the burst, while the load is light

  // ---- the burst: ECG every 4 ms, IMU every 40 ms, PPG FIFO drained every 100 ms ----
  float sx = 0, sy = 0, sz = 0;
  uint32_t imuN = 0;
  const uint32_t periodUs = 1000000UL / Cfg::ECG_SAMPLE_HZ;
  uint32_t next = micros();
  for (uint32_t i = 0; i < Cfg::ECG_SAMPLES; i++)
  {
    next += periodUs;
    r.ecg[i] = ecgInit ? ecg().readSample() : 0;

    if (imuInit && (i % Cfg::IMU_DECIM) == 0 && imuN < Cfg::IMU_SAMPLES)
    {
      float x, y, z;
      if (imu().readAccel(x, y, z))
      {
        float mag = sqrtf(x * x + y * y + z * z);
        r.imu[imuN++] = (int16_t)(mag * 1000.0f);
        sx += x;
        sy += y;
        sz += z;
      }
    }
    if (ppgInit && (i % Cfg::PPG_POLL_TICKS) == 0)
      ppg().poll();

    while ((int32_t)(micros() - next) < 0)
    { /* wait for the next tick */
    }
  }
  r.ecgN = Cfg::ECG_SAMPLES;
  r.imuN = imuN;

  // ---- heart rate ----
  HrResult hr{0, HrQuality::NONE};
  if (ppgInit)
    hr = ppg().finish();
  m.hrBpm = hr.bpm;
  m.hrQuality = hrQualityName(hr.quality);
  m.hrOk = ppgInit;

  // ---- temperature ----
  float t = NAN;
  bool got = tempInit && tempSensor().readC(t);
  m.skinTempC = got ? t : NAN;
  m.tempOk = got && t >= Cfg::TEMP_MIN_C && t <= Cfg::TEMP_MAX_C;

  // ---- quality + summaries ----
  EcgQuality eq = ecgQuality(r.ecg, r.ecgN);
  m.ecgSamples = r.ecgN;
  m.ecgRateHz = Cfg::ECG_SAMPLE_HZ;
  m.burstSec = Cfg::BURST_SEC;
  m.ecgSatPct = eq.satPct;
  m.ecgStd = eq.stdCounts;
  m.ecgOk = ecgInit && eq.ok;

  m.imuSamples = imuN;
  m.imuRateHz = Cfg::IMU_HZ;
  m.imuOk = imuInit && imuN >= (Cfg::IMU_SAMPLES * 8) / 10;
  ImuStats st = imuStats(r.imu, imuN);
  m.meanMagG = st.meanMagG;
  m.rmsDynG = st.rmsDynG;
  m.activity = activityName(st.activity);
  if (imuN > 0)
  {
    m.ax = sx / imuN;
    m.ay = sy / imuN;
    m.az = sz / imuN;
  }

  m.batteryMv = info.batteryMv;
  m.batteryPct = batteryPercent(info.batteryMv);

  r.metaLen = (uint32_t)buildMetaJson(r.meta, sizeof(r.meta), m);
  LOG("burst %lu done: hr=%d(%s) temp=%.1f(%s) ecg_ok=%d sat=%.1f%% activity=%s",
      (unsigned long)seq, m.hrBpm, m.hrQuality, (double)m.skinTempC, m.tempOk ? "ok" : "BAD",
      m.ecgOk, (double)m.ecgSatPct, m.activity);
  return r.metaLen > 0;
}
