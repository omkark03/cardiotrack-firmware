// The whole device in one place:
//
//   wake -> (motion wake? log it, go back to sleep)
//        -> capture 60 s burst (radio off)
//        -> Wi-Fi on -> upload fresh burst -> drain a few queued ones -> Wi-Fi off
//        -> (upload failed? park burst in flash)
//        -> deep sleep until the next burst is due (timer) or motion (MPU6050 INT)
//
#include "app.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "log.h"
#include "state.h"
#include "../hal/factory.h"
#include "../core/payload.h"
#include "../services/burst.h"
#include "../services/net.h"
#include "../services/queue.h"
#include "../services/sleep_mgr.h"

RTC_DATA_ATTR RtcState g_rtc;

namespace App
{

  static const uint32_t RTC_MAGIC = 0xC4D10001;
  static int64_t s_cycleStart = 0; // when this cycle's burst started (system clock)

  static int64_t nowS() { return (int64_t)time(nullptr); }

  // Returns true on a cold boot (first power-up or after a coin-cell swap).
  static bool initState()
  {
    if (g_rtc.magic == RTC_MAGIC)
      return false;
    memset(&g_rtc, 0, sizeof g_rtc);
    g_rtc.magic = RTC_MAGIC;
    Preferences p; // boot counter must survive power loss -> NVS
    p.begin("ct", false);
    g_rtc.bootCount = p.getUInt("boot", 0) + 1;
    p.putUInt("boot", g_rtc.bootCount);
    p.end();
    return true;
  }

  // NTP sync that keeps the schedule consistent when the clock jumps.
  static bool syncClock(int64_t *deltaOut = nullptr, int attempts = 1)
  {
    int64_t before = nowS(); // measure the jump over ALL attempts (a late reply to try 1 counts)
    int64_t ignored = 0;
    bool ok = false;
    for (int i = 0; i < attempts && !ok; i++)
      ok = Net::syncNtp(Cfg::NTP_TIMEOUT_MS, ignored);
    int64_t delta = ok ? nowS() - before : 0;
    if (deltaOut)
      *deltaOut = delta;
    if (ok)
    {
      s_cycleStart += delta;
      if (g_rtc.nextDueS)
        g_rtc.nextDueS += delta;
      g_rtc.cyclesSinceSync = 0;
    }
    return ok;
  }

  static bool clockIsSynced() { return nowS() >= (int64_t)Cfg::MIN_VALID_EPOCH; }

  static void sleepUntilDue(bool armMotion)
  {
    int64_t remain = g_rtc.nextDueS - nowS();
    if (remain < (int64_t)Cfg::MIN_SLEEP_SEC)
      remain = Cfg::MIN_SLEEP_SEC;
    if (remain > (int64_t)Cfg::CYCLE_SEC * Cfg::BAT_LOW_CYCLE_FACTOR)
      remain = Cfg::CYCLE_SEC; // clock went strange
    SleepMgr::sleepFor((uint32_t)remain, armMotion);
  }

  // Send a few queued bursts, oldest first. Stops at the first failure.
  static void flushQueue()
  {
    BurstRecord &r = record();
    char path[64];
    for (uint32_t i = 0; i < Cfg::MAX_FLUSH_PER_CYCLE; i++)
    {
      if (!Queue::loadOldest(r, path, sizeof path))
        return;
      Net::Result res = Net::upload(r);
      if (res == Net::Result::RETRY)
        return;
      if (res == Net::Result::DROP)
        LOG("queue: server rejected %s, discarding", path);
      Queue::remove(path);
      LOG("queue: sent+removed %s (%lu left)", path, (unsigned long)Queue::count());
    }
  }

  static void runCycle(bool coldBoot)
  {
    s_cycleStart = nowS();

    // ---- woken early by motion, burst not due yet: just note it ----
    bool due = coldBoot || nowS() >= g_rtc.nextDueS - 1;
    if (!due)
    {
      g_rtc.motionSinceLast = true;
      LOG("motion wake, next burst in %lld s", (long long)(g_rtc.nextDueS - nowS()));
      sleepUntilDue(false); // one motion note per interval is enough: disarm until the burst
      return;
    }

    // ---- cold boot: get the clock right before the first burst ----
    if (coldBoot && Net::wifiConnect())
    {
      syncClock(nullptr, 2); // the first NTP request often gets lost: try twice
      Net::wifiOff();
    }

    // ---- measure (radio off) ----
    BurstRecord &r = record();
    CaptureInfo info{-1, false};
    uint32_t seq = g_rtc.seq++;
    bool motion = g_rtc.motionSinceLast;
    g_rtc.motionSinceLast = false;
    s_cycleStart = nowS();
    int64_t burstEpoch = nowS(); // burst start on the clock as it was at capture time
    bool built = captureBurst(r, seq, g_rtc.bootCount, motion, info);

    // ---- send ----
    bool handled = false; // true = uploaded, or the server refused it for good
    if (built)
    {
      if (Net::wifiConnect())
      {
        if (!clockIsSynced() || g_rtc.cyclesSinceSync >= Cfg::NTP_RESYNC_CYCLES)
        {
          int64_t delta = 0;
          bool ok = syncClock(&delta, 2);
          if (ok && !info.timeSynced)
          { // burst was captured before the clock was right: fix its timestamp
            size_t n = patchMetaTime(r.meta, r.metaLen, sizeof r.meta, (int64_t)burstEpoch + delta);
            if (n)
            {
              r.metaLen = (uint32_t)n;
              info.timeSynced = true;
              LOG("burst %lu: timestamp corrected", (unsigned long)seq);
            }
          }
        }
        Net::Result res = Net::upload(r);
        handled = (res != Net::Result::RETRY);
        if (res == Net::Result::DROP)
          LOG("burst %lu rejected by server, not queued", (unsigned long)seq);
        if (res == Net::Result::OK)
          flushQueue(); // server is reachable: drain a few old ones
      }
      Net::wifiOff();
      if (!handled)
        Queue::save(r, g_rtc.bootCount, seq);
    }
    g_rtc.cyclesSinceSync++;

    // ---- schedule the next burst, wake-to-wake ----
    uint32_t cycle = Cfg::CYCLE_SEC;
    if (info.batteryMv >= 0 && info.batteryMv < Cfg::BAT_LOW_MV)
    {
      cycle *= Cfg::BAT_LOW_CYCLE_FACTOR;
      LOG("battery low (%d mV): stretching cycle to %lu s", info.batteryMv, (unsigned long)cycle);
    }
    g_rtc.nextDueS = s_cycleStart + cycle;
    sleepUntilDue(true);
  }

  void setup()
  {
    setCpuFrequencyMhz(80); // plenty for 250 Hz sampling, Wi-Fi works at 80 MHz
    Serial.begin(115200);
#ifdef BENCH_MODE
    delay(500);
#endif
    bool cold = initState();
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    bool fsOk = Queue::begin(); // keep out of LOG(): it must run even with -DNO_LOG
    uint32_t queued = fsOk ? Queue::count() : 0;
    LOG("--- CardioTrack %s | %s boot #%lu | wake cause %d | queued %lu ---", FW_VERSION,
        cold ? "COLD" : "warm", (unsigned long)g_rtc.bootCount, (int)cause, (unsigned long)queued);
    runCycle(cold);
  }

  void loop()
  { // bench mode only
    runCycle(false);
  }

} // namespace App
