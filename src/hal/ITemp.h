#pragma once

struct ITemp {
  virtual bool begin() = 0;
  virtual bool readC(float& c) = 0;   // false = electrically implausible (open/short)
  virtual ~ITemp() {}
};
