// Runs the REAL firmware code (app, burst, queue, uploader, sleep manager) on the laptop
// against stubbed hardware, with scripted failures, and checks the behaviour.
//
//   built with -DBENCH_MODE   -> "bench" scenario: Wi-Fi outage, server errors, bad record, NTP outage
//   built without BENCH_MODE  -> "sleep" scenario: real deep-sleep path, motion wake, outage, power loss
//   built with -DSIM_NO_SENSORS (real drivers on an empty I2C bus) -> sensors fail gracefully
#include <algorithm>
#include <set>
#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "config.h"
#include "app/state.h"

void setup();
void loop();

static int fails = 0;
static void check(bool c, const char* msg) { printf("  [%s] %s\n", c ? "PASS" : "FAIL", msg); if (!c) fails++; }

static bool okUpload(const SimUpload& u) { return u.code >= 200 && u.code < 300; }

static std::vector<SimUpload> successes() {
  std::vector<SimUpload> v;
  for (auto& u : g_sim_uploads) if (okUpload(u)) v.push_back(u);
  return v;
}

static long queuedFiles() {
  long n = 0; std::error_code ec;
  for (auto& e : std::filesystem::directory_iterator("sim_flash/q", ec)) if (e.is_regular_file()) n++;
  return n;
}

static int posOf(const std::vector<SimUpload>& v, long boot, long seq) {
  for (size_t i = 0; i < v.size(); i++) if (v[i].boot == boot && v[i].seq == seq) return (int)i;
  return -1;
}

#ifdef BENCH_MODE
// ---------------------------------------------------------------------------------------
static void runBench() {
  printf("\n=== BENCH scenario: 16 cycles ===\n"
         "  cycles 0-2 : NTP unreachable (clock stays unsynced)\n"
         "  cycles 3-5 : Wi-Fi down (bursts must be queued)\n"
         "  cycles 8-9 : server returns 503 (queued)\n"
         "  cycle 10   : server returns 400 (record rejected for good)\n\n");
  const int N = 16;
  for (int c = 0; c < N; c++) {
    g_sim_ntp_allowed = (c >= 2);
    g_sim_wifi_ok = !(c >= 3 && c <= 5);
    g_sim_http_code = (c == 8 || c == 9) ? 503 : (c == 10 ? 400 : 200);
    if (c == 0) setup(); else loop();
  }

  auto ok = successes();
  printf("\n--- checks ---\n");
  std::set<long> seen; bool dup = false;
  for (auto& u : ok) { if (!seen.insert(u.seq).second) dup = true; }
  std::set<long> expect; for (long s = 0; s < N; s++) if (s != 10) expect.insert(s);
  check(!dup, "no burst was delivered twice");
  check(seen == expect, "every burst delivered exactly once, except #10 (rejected with 400)");
  check(queuedFiles() == 0, "flash queue is empty at the end");
  check(posOf(ok, 1, 3) < posOf(ok, 1, 4) && posOf(ok, 1, 4) < posOf(ok, 1, 5), "queued bursts 3,4,5 went out oldest-first");
  check(posOf(ok, 1, 6) < posOf(ok, 1, 3), "fresh burst #6 sent before draining the backlog");
  check(posOf(ok, 1, 8) >= 0 && posOf(ok, 1, 9) >= 0, "bursts queued during the 503 outage were delivered later");
  bool syncOk = true;
  for (auto& u : ok) syncOk &= (u.synced == (u.seq >= 3));
  check(syncOk, "bursts 0-2 flagged time_synced=false, 3+ flagged true (clock synced during cycle 2)");
  bool allBlobs = true; for (auto& u : ok) allBlobs &= u.hasEcg;
  check(allBlobs, "every upload carried meta + ecg + imu parts");
#ifdef SIM_NO_SENSORS
  bool flagged = !ok.empty();
  for (auto& u : ok) flagged &= u.meta.find("\"imu\":{\"sample_rate_hz\":25,\"samples\":0,\"ok\":false") != std::string::npos &&
                               u.meta.find("\"hr\":{\"bpm\":0,\"signal_quality\":\"none\",\"ok\":false}") != std::string::npos;
  check(flagged, "missing IMU and PPG are reported as ok=false, firmware kept running");
#endif
}
#else
// ---------------------------------------------------------------------------------------
static void runSleep() {
  printf("\n=== SLEEP scenario: real deep-sleep path, 60 s burst / 300 s cycle ===\n"
         "  wake 3     : motion wake (halfway through the interval)\n"
         "  wakes 5-6  : Wi-Fi down\n"
         "  wake 9     : coin-cell swap (power loss, cold boot)\n\n");
  const int W = 14;
  esp_sleep_wakeup_cause_t cause = ESP_SLEEP_WAKEUP_UNDEFINED;
  bool motionDisarmedAfterMotion = false;
  bool motionWakeHappened = false;
  uint32_t lastSleep = 0;

  for (int w = 0; w < W; w++) {
    g_sim_wifi_ok = !(w == 5 || w == 6);
    if (w == 9) {                                        // power loss: RTC memory + clock gone, NVS stays
      memset(&g_rtc, 0, sizeof g_rtc);
      g_vt_us = 0; g_sim_epoch_base = 0; cause = ESP_SLEEP_WAKEUP_UNDEFINED;
    }
    g_sim_cause = cause;
    g_sim_ext0 = false;
    printf("---- wake %d (cause %d) ----\n", w, (int)cause);
    try {
      setup();
      printf("  (setup returned without sleeping?)\n"); fails++;
      break;
    } catch (SimDeepSleep& s) {
      lastSleep = s.seconds;
      if (motionWakeHappened && !motionDisarmedAfterMotion) {
        motionDisarmedAfterMotion = !s.motionArmed;      // first sleep after the motion wake
      }
      bool injectMotion = (w == 2) && s.motionArmed && s.seconds > 20;
      if (injectMotion) {
        g_vt_us += (uint64_t)(s.seconds / 2) * 1000000ULL;
        cause = ESP_SLEEP_WAKEUP_EXT0;
        motionWakeHappened = true;
      } else {
        g_vt_us += (uint64_t)s.seconds * 1000000ULL;
        cause = ESP_SLEEP_WAKEUP_TIMER;
      }
    }
  }
  (void)lastSleep;

  auto ok = successes();
  printf("\n--- checks ---\n");
  std::set<std::pair<long, long>> seen; bool dup = false;
  for (auto& u : ok) if (!seen.insert({u.boot, u.seq}).second) dup = true;
  check(!dup, "no burst delivered twice");
  check(queuedFiles() == 0, "flash queue is empty at the end");
  check(motionWakeHappened && motionDisarmedAfterMotion, "after a motion wake the firmware sleeps again with motion disarmed");

  // bursts per boot
  long b1 = 0, b2 = 0, maxBoot = 0;
  for (auto& u : ok) { if (u.boot == 1) b1++; if (u.boot == 2) b2++; maxBoot = std::max(maxBoot, u.boot); }
  check(maxBoot == 2, "boot_count went 1 -> 2 after the simulated power loss (NVS survived)");
  check(b1 > 0 && b2 > 0 && posOf(ok, 2, 0) >= 0, "seq restarted at 0 after the cold boot, uploads continued");

  // motion flag: exactly the first burst after the motion wake carries it
  long motionBursts = 0, motionSeq = -1;
  for (auto& u : ok) if (u.motion) { motionBursts++; motionSeq = u.seq; }
  check(motionBursts == 1, "exactly one burst carries motion_since_last=true");
  printf("      (motion flag was on burst #%ld)\n", motionSeq);

  // schedule: consecutive synced bursts are one cycle apart, even across the motion wake and the outage
  std::vector<SimUpload> boot1;
  for (auto& u : ok) if (u.boot == 1 && u.synced) boot1.push_back(u);
  std::sort(boot1.begin(), boot1.end(), [](const SimUpload& a, const SimUpload& b) { return a.seq < b.seq; });
  bool spaced = boot1.size() > 3;
  for (size_t i = 1; i < boot1.size(); i++) {
    long long d = boot1[i].epoch - boot1[i - 1].epoch;
    if (boot1[i].seq == boot1[i - 1].seq + 1 && (d < (long long)Cfg::CYCLE_SEC - 3 || d > (long long)Cfg::CYCLE_SEC + 3)) {
      printf("      seq %ld -> %ld spacing %lld s\n", boot1[i - 1].seq, boot1[i].seq, d); spaced = false;
    }
  }
  check(spaced, "synced bursts are 300 s apart (+-3 s): motion wake and Wi-Fi outage did not shift the schedule");
  bool syncedLater = true;
  for (auto& u : ok) if (u.boot == 1 && u.seq >= 1) syncedLater &= u.synced;
  check(syncedLater && !ok.empty() && ok.front().synced, "clock was synced on the cold boot before the first burst");
}
#endif

int main(int argc, char** argv) {
  g_sim_dump_dir = argc > 1 ? argv[1] : nullptr;
  std::filesystem::remove_all("sim_flash");
  srand(7);
#ifdef BENCH_MODE
  runBench();
#else
  runSleep();
#endif
  printf("\nuploads attempted: %zu, accepted: %zu, ntp syncs: %d\n", g_sim_uploads.size(), successes().size(), g_sim_ntp_syncs);
  printf("%s (%d failure%s)\n", fails ? "SIMULATION FAILED" : "SIMULATION PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
