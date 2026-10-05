#pragma once
#include "Arduino.h"
#include "WiFi.h"

// One entry per HTTP POST the firmware makes. Meta fields are parsed out of the multipart body.
struct SimUpload { long boot = -1, seq = -1; long long epoch = 0; bool synced = false, motion = false; int code = 0; size_t bytes = 0; bool hasEcg = false; std::string meta; };
inline std::vector<SimUpload> g_sim_uploads;
inline int g_sim_http_code = 200;            // scripted server response (negative = connection error)
inline const char* g_sim_dump_dir = nullptr; // if set, each body is written there as rec_<n>.bin

class HTTPClient {
public:
  void setTimeout(uint32_t) {}
  void setReuse(bool) {}
  bool begin(WiFiClient&, const char*) { return true; }
  void addHeader(const char*, const String&) {}
  int POST(uint8_t* body, size_t len) {
    SimUpload u; u.bytes = len; u.code = g_sim_http_code;
    std::string s((const char*)body, len);
    auto num = [&](const char* key) -> long { size_t p = s.find(key); return p == std::string::npos ? -1 : atol(s.c_str() + p + strlen(key)); };
    u.boot = num("\"boot_count\":");
    u.seq = num("\"seq\":");
    u.epoch = num("\"epoch\":");
    size_t m0 = s.find("\r\n\r\n"), m1 = s.find("\r\n--", m0 == std::string::npos ? 0 : m0);
    if (m0 != std::string::npos && m1 != std::string::npos) u.meta = s.substr(m0 + 4, m1 - m0 - 4);
    u.synced = s.find("\"time_synced\":true") != std::string::npos;
    u.motion = s.find("\"motion_since_last\":true") != std::string::npos;
    u.hasEcg = s.find("name=\"ecg\"") != std::string::npos && s.find("name=\"imu\"") != std::string::npos;
    if (g_sim_dump_dir) {
      char p[256]; snprintf(p, sizeof p, "%s/rec_%03zu.bin", g_sim_dump_dir, g_sim_uploads.size());
      FILE* f = fopen(p, "wb"); if (f) { fwrite(body, 1, len, f); fclose(f); }
    }
    g_sim_uploads.push_back(u);
    delay(300);                                                          // 0.3 s on the wire
    return g_sim_http_code;
  }
  void end() {}
};
