#pragma once
#include <stdint.h>

// Survives deep sleep (RTC memory). Lost on power loss (coin-cell swap) -> "cold boot".
struct RtcState {
  uint32_t magic;
  uint32_t bootCount;        // from NVS, +1 on every cold boot (persists across power loss)
  uint32_t seq;              // burst counter since the last cold boot
  int64_t  nextDueS;         // system-clock time the next burst is due
  uint32_t cyclesSinceSync;  // bursts since the last NTP sync
  bool     motionSinceLast;  // motion wake-up seen since the previous burst
  bool     haveBssid;        // cached Wi-Fi channel + BSSID for fast reconnect
  uint8_t  wifiChannel;
  uint8_t  bssid[6];
};

extern RtcState g_rtc;
