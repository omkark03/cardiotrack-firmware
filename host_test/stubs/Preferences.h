#pragma once
#include "Arduino.h"

// NVS stand-in: kept in memory for the whole simulated run, so it "survives power loss".
inline std::map<std::string, uint32_t> g_sim_nvs;

class Preferences {
public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  uint32_t getUInt(const char* k, uint32_t def = 0) { auto it = g_sim_nvs.find(k); return it == g_sim_nvs.end() ? def : it->second; }
  size_t putUInt(const char* k, uint32_t v) { g_sim_nvs[k] = v; return 4; }
};
