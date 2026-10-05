#pragma once
#include <stddef.h>
#include <stdint.h>
#include "record.h"

// Outage buffer in LittleFS flash. One file per burst, oldest sent first.
namespace Queue {
  bool     begin();                                              // mount (formats on first use)
  uint32_t count();
  bool     save(const BurstRecord& r, uint32_t bootCount, uint32_t seq);   // drops oldest if full
  bool     loadOldest(BurstRecord& r, char* pathOut, size_t cap);          // false = empty
  void     remove(const char* path);
}
