#include "net.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_sntp.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "log.h"
#include "secrets.h"
#include "../app/state.h"
#include "../core/payload.h"

namespace Net
{

  static const char *BOUNDARY = "----CardioTrackBoundary7MA4YWxk";

  bool wifiConnect()
  {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(CFG_WIFI_TX_POWER);

    bool cached = g_rtc.haveBssid;
    if (cached)
      WiFi.begin(WIFI_SSID, WIFI_PASS, g_rtc.wifiChannel, g_rtc.bssid);
    else
      WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      uint32_t el = millis() - start;
      if (el > Cfg::WIFI_CONNECT_TIMEOUT_MS)
        break;
      if (cached && el > 4000)
      { // cached AP info did not work (moved?): plain scan
        cached = false;
        g_rtc.haveBssid = false;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
      }
      delay(50);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      LOG("wifi: connect failed after %lu ms", (unsigned long)(millis() - start));
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    g_rtc.wifiChannel = (uint8_t)WiFi.channel();
    memcpy(g_rtc.bssid, WiFi.BSSID(), 6);
    g_rtc.haveBssid = true;
    LOG("wifi: connected in %lu ms", (unsigned long)(millis() - start));
    return true;
  }

  void wifiOff()
  {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  bool syncNtp(uint32_t timeoutMs, int64_t &deltaS)
  {
    int64_t before = (int64_t)time(nullptr);
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTime(0, 0, "pool.ntp.org", "time.google.com");

    // ESP-IDF reports COMPLETED only once: reading the status resets it. So read it exactly
    // once per pass and remember the answer (reading it again would say "not synced").
    bool ok = false;
    uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
      if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
      {
        ok = true;
        break;
      }
      delay(50);
    }
    // Safety net: the clock went from "never set" to a valid date, so a sync did happen.
    if (!ok && before < (int64_t)Cfg::MIN_VALID_EPOCH && (int64_t)time(nullptr) >= (int64_t)Cfg::MIN_VALID_EPOCH)
      ok = true;
    deltaS = ok ? (int64_t)time(nullptr) - before : 0;
    LOG("ntp: %s (clock moved %lld s)", ok ? "synced" : "FAILED", (long long)deltaS);
    return ok;
  }

  Result upload(const BurstRecord &r)
  {
    size_t need = multipartSize(BOUNDARY, r.metaLen, (size_t)r.ecgN * 2, (size_t)r.imuN * 2);
    uint8_t *body = (uint8_t *)malloc(need);
    if (!body)
    {
      LOG("upload: cannot allocate %u bytes", (unsigned)need);
      return Result::RETRY;
    }
    size_t len = buildMultipart(body, need, BOUNDARY, r.meta, r.metaLen, r.ecg, r.ecgN, r.imu, r.imuN);
    if (len == 0)
    {
      free(body);
      LOG("upload: body build failed");
      return Result::DROP;
    }

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(Cfg::HTTP_TIMEOUT_MS);
    http.setReuse(false);
    if (!http.begin(client, BACKEND_URL))
    {
      free(body);
      return Result::RETRY;
    }
    http.addHeader("Content-Type", String("multipart/form-data; boundary=") + BOUNDARY);
    http.addHeader("X-Device-Key", DEVICE_KEY);

    int code = http.POST(body, len);
    http.end();
    free(body);
    LOG("upload: %u bytes -> HTTP %d", (unsigned)len, code);

    if (code >= 200 && code < 300)
      return Result::OK;
    if (code == 400 || code == 413 || code == 422)
      return Result::DROP;
    return Result::RETRY;
  }

} // namespace Net