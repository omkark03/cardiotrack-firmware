#pragma once
// Laptop stand-in for the Arduino/ESP32 core, just enough to compile and SIMULATE the firmware.
// Time is virtual: it advances only when the firmware waits, so a 60 s burst takes milliseconds.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>

#define RTC_DATA_ATTR
#define INPUT 0
#define ADC_11db 3

inline uint64_t g_vt_us = 0;                 // virtual microseconds since "power-up"
inline int64_t  g_sim_epoch_base = 0;        // added to virtual seconds; set by a fake NTP sync

inline uint32_t micros() { g_vt_us += 40; return (uint32_t)g_vt_us; }   // each call costs 40 us
inline uint32_t millis() { return (uint32_t)(g_vt_us / 1000); }
inline void delay(uint32_t ms) { g_vt_us += (uint64_t)ms * 1000; }
inline void delayMicroseconds(uint32_t us) { g_vt_us += us; }
inline long random(long a, long b) { return a + rand() % (b - a); }
inline void setCpuFrequencyMhz(int) {}

// analog / gpio (values are scripted by the simulator)
inline int g_sim_adc_mv = 1650;
inline void pinMode(int, int) {}
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline int  analogRead(int) { return 2048; }
inline uint32_t analogReadMilliVolts(int) { return g_sim_adc_mv; }

// system clock: seconds since power-up until a fake NTP sync moves it to "real" time
inline time_t sim_time(time_t* t) { time_t v = (time_t)(g_sim_epoch_base + (int64_t)(g_vt_us / 1000000)); if (t) *t = v; return v; }
#define time(x) sim_time(x)

struct SerialStub {
  void begin(int) {}
  void flush() {}
  int printf(const char* f, ...) __attribute__((format(printf, 2, 3))) {
    va_list a; va_start(a, f); int n = vprintf(f, a); va_end(a); return n;
  }
  void println() { puts(""); }
  void println(const char* s) { puts(s); }
  void println(int v) { ::printf("%d\n", v); }
};
inline SerialStub Serial;

class String : public std::string {
public:
  String(const char* s = "") : std::string(s) {}
  String(const std::string& s) : std::string(s) {}
};
inline String operator+(const String& a, const char* b) { return String(std::string(a) + b); }

// configTime(): "starts SNTP"; the simulator decides whether it succeeds
inline bool g_sim_ntp_requested = false;
inline void configTime(long, int, const char*, const char* = nullptr) { g_sim_ntp_requested = true; }
