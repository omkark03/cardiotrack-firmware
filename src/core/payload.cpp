#include "payload.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// int16 arrays are copied as raw bytes. ESP32 (and x86) are little-endian, which is
// what the backend expects ("<i2").

static void isoTime(char *out, size_t cap, int64_t epoch)
{
  time_t t = (time_t)epoch;
  struct tm tmv;
  gmtime_r(&t, &tmv);
  strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

static int bstr(bool b, char *out, size_t cap) { return snprintf(out, cap, "%s", b ? "true" : "false"); }

size_t buildMetaJson(char *out, size_t cap, const Meta &m)
{
  char ts[32];
  isoTime(ts, sizeof(ts), m.epoch);
  char temp[16];
  if (isnan(m.skinTempC))
    snprintf(temp, sizeof(temp), "null");
  else
    snprintf(temp, sizeof(temp), "%.2f", m.skinTempC);

  char b1[8], b2[8], b3[8], b4[8], b5[8], b6[8];
  bstr(m.timeSynced, b1, sizeof b1);
  bstr(m.ecgOk, b2, sizeof b2);
  bstr(m.imuOk, b3, sizeof b3);
  bstr(m.hrOk, b4, sizeof b4);
  bstr(m.tempOk, b5, sizeof b5);
  bstr(m.motionSinceLast, b6, sizeof b6);

  int n = snprintf(out, cap,
                   "{\"device_id\":\"%s\",\"fw\":\"%s\",\"seq\":%lu,\"boot_count\":%lu,"
                   "\"timestamp\":\"%s\",\"epoch\":%lld,\"time_synced\":%s,"
                   "\"ecg\":{\"sample_rate_hz\":%lu,\"burst_duration_sec\":%lu,\"samples\":%lu,"
                   "\"sat_pct\":%.2f,\"std\":%.1f,\"ok\":%s},"
                   "\"imu\":{\"sample_rate_hz\":%lu,\"samples\":%lu,\"ok\":%s,"
                   "\"accel_x\":%.3f,\"accel_y\":%.3f,\"accel_z\":%.3f,"
                   "\"mean_mag_g\":%.3f,\"rms_dyn_g\":%.3f,\"activity_level\":\"%s\","
                   "\"motion_since_last\":%s},"
                   "\"hr\":{\"bpm\":%d,\"signal_quality\":\"%s\",\"ok\":%s},"
                   "\"skin_temp_c\":%s,\"temp_ok\":%s,"
                   "\"battery\":{\"mv\":%d,\"chest_pct\":%d}}",
                   m.deviceId, m.fwVersion, (unsigned long)m.seq, (unsigned long)m.bootCount,
                   ts, (long long)m.epoch, b1,
                   (unsigned long)m.ecgRateHz, (unsigned long)m.burstSec, (unsigned long)m.ecgSamples,
                   (double)m.ecgSatPct, (double)m.ecgStd, b2,
                   (unsigned long)m.imuRateHz, (unsigned long)m.imuSamples, b3,
                   (double)m.ax, (double)m.ay, (double)m.az,
                   (double)m.meanMagG, (double)m.rmsDynG, m.activity, b6,
                   m.hrBpm, m.hrQuality, b4,
                   temp, b5,
                   m.batteryMv, m.batteryPct);

  if (n < 0 || (size_t)n >= cap)
    return 0;
  return (size_t)n;
}

size_t patchMetaTime(char *meta, size_t len, size_t cap, int64_t newEpoch)
{
  // buildMetaJson writes  ..."timestamp":"<ts>","epoch":<n>,"time_synced":<b>,"ecg":{...  contiguously.
  static const char *kHead = "\"timestamp\":\"";
  static const char *kTail = "\"ecg\":";
  char *a = strstr(meta, kHead);
  char *b = strstr(meta, kTail);
  if (!a || !b || b < a)
    return 0;
  char ts[32];
  isoTime(ts, sizeof ts, newEpoch);
  char seg[128];
  int n = snprintf(seg, sizeof seg, "\"timestamp\":\"%s\",\"epoch\":%lld,\"time_synced\":true,\"time_estimated\":true,",
                   ts, (long long)newEpoch);
  if (n <= 0 || (size_t)n >= sizeof seg)
    return 0;
  size_t tailLen = len - (size_t)(b - meta); // from "ecg": to the end
  size_t newLen = (size_t)(a - meta) + (size_t)n + tailLen;
  if (newLen + 1 > cap)
    return 0;
  memmove(a + n, b, tailLen);
  memcpy(a, seg, (size_t)n);
  meta[newLen] = 0;
  return newLen;
}

// ---- multipart ----

static int headFor(char *out, size_t cap, const char *boundary, const char *name, const char *filename)
{
  if (filename)
    return snprintf(out, cap,
                    "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
                    "Content-Type: application/octet-stream\r\n\r\n",
                    boundary, name, filename);
  return snprintf(out, cap,
                  "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n"
                  "Content-Type: application/json\r\n\r\n",
                  boundary, name);
}

size_t multipartSize(const char *boundary, size_t metaLen, size_t ecgBytes, size_t imuBytes)
{
  char tmp[256];
  size_t total = 0;
  total += (size_t)headFor(tmp, sizeof tmp, boundary, "meta", nullptr) + metaLen + 2;
  total += (size_t)headFor(tmp, sizeof tmp, boundary, "ecg", "ecg.bin") + ecgBytes + 2;
  total += (size_t)headFor(tmp, sizeof tmp, boundary, "imu", "imu.bin") + imuBytes + 2;
  total += (size_t)snprintf(tmp, sizeof tmp, "--%s--\r\n", boundary);
  return total;
}

size_t buildMultipart(uint8_t *out, size_t cap, const char *boundary,
                      const char *meta, size_t metaLen,
                      const int16_t *ecg, size_t ecgN,
                      const int16_t *imu, size_t imuN)
{
  size_t need = multipartSize(boundary, metaLen, ecgN * 2, imuN * 2);
  if (need > cap)
    return 0;

  size_t p = 0;
  char head[256];
  int h;

  h = headFor(head, sizeof head, boundary, "meta", nullptr);
  memcpy(out + p, head, h);
  p += h;
  memcpy(out + p, meta, metaLen);
  p += metaLen;
  out[p++] = '\r';
  out[p++] = '\n';

  h = headFor(head, sizeof head, boundary, "ecg", "ecg.bin");
  memcpy(out + p, head, h);
  p += h;
  memcpy(out + p, ecg, ecgN * 2);
  p += ecgN * 2;
  out[p++] = '\r';
  out[p++] = '\n';

  h = headFor(head, sizeof head, boundary, "imu", "imu.bin");
  memcpy(out + p, head, h);
  p += h;
  memcpy(out + p, imu, imuN * 2);
  p += imuN * 2;
  out[p++] = '\r';
  out[p++] = '\n';

  h = snprintf(head, sizeof head, "--%s--\r\n", boundary);
  memcpy(out + p, head, h);
  p += h;
  return p;
}
