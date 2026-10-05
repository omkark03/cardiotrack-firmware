#pragma once
#include <stdint.h>
#include "record.h"

namespace Net {
  enum class Result { OK, DROP, RETRY };

  bool wifiConnect();          // fast reconnect using the cached channel/BSSID when possible
  void wifiOff();

  // Needs Wi-Fi. Re-syncs the system clock. deltaS = how far the clock moved.
  bool syncNtp(uint32_t timeoutMs, int64_t& deltaS);

  // OK    = server accepted it
  // DROP  = server says the record itself is bad (400/413/422): resending will never work
  // RETRY = anything else (no connection, 5xx, wrong key, ...): keep the data and try later
  Result upload(const BurstRecord& r);
}
