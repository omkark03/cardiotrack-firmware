#pragma once
#include "../IBattery.h"

// Used when there is no battery divider (breadboard on USB). Reports "unknown".
class NoBattery : public IBattery {
public:
  bool begin() override { return true; }
  int  readMv() override { return -1; }
};
