#pragma once

namespace App {
  void setup();   // called from Arduino setup(): does one full cycle, then deep sleeps
  void loop();    // only reached with -DBENCH_MODE (no deep sleep)
}
