#pragma once
#include "Arduino.h"

inline bool g_sim_wifi_ok = true;            // scripted: is the access point reachable?

enum wl_status_t { WL_IDLE_STATUS, WL_CONNECTED, WL_DISCONNECTED };
enum WiFiMode_t { WIFI_OFF, WIFI_STA };
enum wifi_power_t { WIFI_POWER_15dBm };

class WiFiClass {
  bool begun = false;
  WiFiMode_t m = WIFI_OFF;
public:
  void persistent(bool) {}
  bool mode(WiFiMode_t x) { m = x; if (x == WIFI_OFF) begun = false; return true; }
  bool setTxPower(wifi_power_t) { return true; }
  void begin(const char*, const char*, int32_t = 0, const uint8_t* = nullptr) { begun = true; }
  wl_status_t status() { return (begun && m == WIFI_STA && g_sim_wifi_ok) ? WL_CONNECTED : WL_DISCONNECTED; }
  int32_t channel() { return 6; }
  uint8_t* BSSID() { static uint8_t b[6] = {1, 2, 3, 4, 5, 6}; return b; }
  bool disconnect(bool = false) { begun = false; return true; }
  bool isOn() const { return m != WIFI_OFF; }
};
inline WiFiClass WiFi;

class WiFiClient {};
