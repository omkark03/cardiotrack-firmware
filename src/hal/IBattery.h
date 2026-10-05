#pragma once

struct IBattery {
  virtual bool begin() = 0;
  virtual int  readMv() = 0;   // cell voltage in mV, or -1 if unknown
  virtual ~IBattery() {}
};
