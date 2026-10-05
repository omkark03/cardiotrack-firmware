#pragma once
#include "Arduino.h"
#include "WiFi.h"

enum sntp_sync_status_t { SNTP_SYNC_STATUS_RESET, SNTP_SYNC_STATUS_IN_PROGRESS, SNTP_SYNC_STATUS_COMPLETED };

inline sntp_sync_status_t g_sim_sntp = SNTP_SYNC_STATUS_RESET;
inline bool g_sim_ntp_allowed = true;        // scripted: is the NTP server reachable?
inline int  g_sim_ntp_syncs = 0;

inline void sntp_set_sync_status(sntp_sync_status_t s) { g_sim_sntp = s; g_sim_ntp_requested = false; }

inline sntp_sync_status_t sntp_get_sync_status() {
  if (g_sim_sntp != SNTP_SYNC_STATUS_COMPLETED && g_sim_ntp_requested && g_sim_ntp_allowed && g_sim_wifi_ok) {
    g_sim_epoch_base = 1790000000LL - (int64_t)(g_vt_us / 1000000);     // clock jumps to "real" time
    g_sim_sntp = SNTP_SYNC_STATUS_COMPLETED;
    g_sim_ntp_syncs++;
  }
  return g_sim_sntp;
}
