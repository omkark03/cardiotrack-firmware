// Laptop-side tests for the portable firmware modules (no ESP32 needed).
//   make -C host_test            builds + runs
// Also writes a real multipart body (body.bin) for testing backend_fake/server.py.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "core/hr_algo.h"
#include "core/quality.h"
#include "core/payload.h"
#include "config.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

// reflective PPG: DC level minus a pulse bump each beat (so peaks point down), plus noise
static std::vector<int32_t> makePpg(float bpm, float seconds, float fs, int32_t dc, float amp, float noise) {
  int n = (int)(seconds * fs);
  std::vector<int32_t> v(n);
  float period = 60.0f / bpm;
  for (int i = 0; i < n; i++) {
    float t = fmodf(i / fs, period) / period;
    float pulse = expf(-0.5f * powf((t - 0.25f) / 0.08f, 2)) + 0.35f * expf(-0.5f * powf((t - 0.55f) / 0.10f, 2));
    float nz = ((rand() % 2001) - 1000) / 1000.0f * noise;
    v[i] = (int32_t)(dc - amp * pulse + nz);
  }
  return v;
}

static float gauss(float t, float mu, float sigma, float amp) { float d = (t - mu) / sigma; return amp * expf(-0.5f * d * d); }

int main(int argc, char** argv) {
  const char* outdir = argc > 1 ? argv[1] : ".";
  srand(1);

  // ---- heart rate ----
  const float fs = 25.0f;
  for (float bpm : {55.0f, 72.0f, 110.0f}) {
    auto ppg = makePpg(bpm, 58, fs, 60000, 900, 120);
    HrResult r = computeHr(ppg.data(), (int)ppg.size(), fs, Cfg::PPG_MIN_DC);
    char msg[96];
    snprintf(msg, sizeof msg, "HR %.0f bpm -> got %d (%s)", bpm, r.bpm, hrQualityName(r.quality));
    CHECK(fabsf(r.bpm - bpm) <= 2 && r.quality == HrQuality::GOOD, msg);
  }
  {
    auto ppg = makePpg(72, 58, fs, 2000, 900, 50);                 // LED barely reflecting: no skin
    HrResult r = computeHr(ppg.data(), (int)ppg.size(), fs, Cfg::PPG_MIN_DC);
    CHECK(r.quality == HrQuality::NONE && r.bpm == 0, "HR: no skin contact -> NONE");
  }
  {
    auto ppg = makePpg(72, 58, fs, 60000, 0, 60);                  // good DC, noise only
    HrResult r = computeHr(ppg.data(), (int)ppg.size(), fs, Cfg::PPG_MIN_DC);
    CHECK(r.quality != HrQuality::GOOD, "HR: noise only is never GOOD");
  }
  {
    auto ppg = makePpg(72, 5, fs, 60000, 900, 50);                 // too short
    HrResult r = computeHr(ppg.data(), (int)ppg.size(), fs, Cfg::PPG_MIN_DC);
    CHECK(r.quality == HrQuality::NONE, "HR: <8 s of data -> NONE");
  }

  // ---- ECG quality ----
  std::vector<int16_t> ecg(Cfg::ECG_SAMPLES);
  for (uint32_t i = 0; i < ecg.size(); i++) {
    float t = fmodf((float)i / Cfg::ECG_SAMPLE_HZ, 0.8f) / 0.8f;
    float v = gauss(t, .20f, .025f, .12f) + gauss(t, .36f, .01f, -.12f) + gauss(t, .40f, .012f, 1.f) +
              gauss(t, .44f, .01f, -.25f) + gauss(t, .65f, .045f, .30f) + ((rand() % 201) - 100) / 3000.0f;
    ecg[i] = (int16_t)(1800 + v * 1200);
  }
  CHECK(ecgQuality(ecg.data(), ecg.size()).ok, "ECG quality: clean synthetic ECG ok");
  std::vector<int16_t> railed(1000, 4095), flat(1000, 1800);
  CHECK(!ecgQuality(railed.data(), railed.size()).ok, "ECG quality: railed ADC flagged");
  CHECK(!ecgQuality(flat.data(), flat.size()).ok, "ECG quality: flat line flagged");

  // ---- IMU stats ----
  std::vector<int16_t> still(Cfg::IMU_SAMPLES), walk(Cfg::IMU_SAMPLES);
  for (uint32_t i = 0; i < still.size(); i++) {
    still[i] = (int16_t)(1000 + (rand() % 21) - 10);
    walk[i]  = (int16_t)(1000 + 300 * sinf(2 * 3.14159f * 2 * i / Cfg::IMU_HZ));
  }
  CHECK(imuStats(still.data(), still.size()).activity == Activity::REST, "IMU: still -> resting");
  CHECK(imuStats(walk.data(), walk.size()).activity == Activity::ACTIVE, "IMU: 0.3 g oscillation -> active");

  // ---- battery ----
  CHECK(batteryPercent(-1) == -1 && batteryPercent(3000) == 100 && batteryPercent(2000) == 0, "battery: endpoints");
  CHECK(batteryPercent(2850) > batteryPercent(2750), "battery: monotonic");

  // ---- payload ----
  Meta m{};
  m.deviceId = DEVICE_ID; m.fwVersion = FW_VERSION; m.seq = 7; m.bootCount = 1;
  m.epoch = 1790000000; m.timeSynced = true;
  m.ecgSamples = ecg.size(); m.ecgRateHz = Cfg::ECG_SAMPLE_HZ; m.burstSec = Cfg::BURST_SEC;
  m.ecgSatPct = 0; m.ecgStd = 400; m.ecgOk = true;
  m.imuSamples = still.size(); m.imuRateHz = Cfg::IMU_HZ; m.imuOk = true;
  m.meanMagG = 1.0f; m.rmsDynG = 0.005f; m.ax = 0.01f; m.ay = 0.0f; m.az = 1.0f; m.activity = "resting";
  m.motionSinceLast = false;
  m.hrBpm = 74; m.hrQuality = "good"; m.hrOk = true;
  m.skinTempC = 36.62f; m.tempOk = true;
  m.batteryMv = -1; m.batteryPct = -1;

  char meta[Cfg::META_MAX];
  size_t ml = buildMetaJson(meta, sizeof meta, m);
  CHECK(ml > 0 && ml < sizeof meta, "payload: metadata JSON fits buffer");
  printf("      meta (%zu bytes): %s\n", ml, meta);

  Meta bad = m; bad.skinTempC = NAN;
  char meta2[Cfg::META_MAX];
  buildMetaJson(meta2, sizeof meta2, bad);
  CHECK(strstr(meta2, "\"skin_temp_c\":null") != nullptr, "payload: NaN temperature becomes null");

  const char* boundary = "----CardioTrackBoundary7MA4YWxk";
  size_t need = multipartSize(boundary, ml, ecg.size() * 2, still.size() * 2);
  std::vector<uint8_t> body(need);
  size_t got = buildMultipart(body.data(), body.size(), boundary, meta, ml, ecg.data(), ecg.size(), still.data(), still.size());
  CHECK(got == need, "payload: multipart size matches prediction");
  std::vector<uint8_t> tiny(100);
  CHECK(buildMultipart(tiny.data(), tiny.size(), boundary, meta, ml, ecg.data(), ecg.size(), still.data(), still.size()) == 0,
        "payload: too-small buffer is refused");

  char path[512];
  snprintf(path, sizeof path, "%s/body.bin", outdir);
  FILE* f = fopen(path, "wb"); fwrite(body.data(), 1, got, f); fclose(f);
  snprintf(path, sizeof path, "%s/boundary.txt", outdir);
  f = fopen(path, "w"); fputs(boundary, f); fclose(f);
  printf("      wrote body.bin (%zu bytes) + boundary.txt to %s\n", got, outdir);

  printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
