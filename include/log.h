#pragma once
#include <Arduino.h>

// LOG("text %d", x) -> prints a line to Serial. Build with -DNO_LOG to remove.
#ifdef NO_LOG
  #define LOG(...) do {} while (0)
#else
  #define LOG(...) do { Serial.printf(__VA_ARGS__); Serial.println(); } while (0)
#endif
