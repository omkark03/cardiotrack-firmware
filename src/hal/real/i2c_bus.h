#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "board_pins.h"

// Shared I2C helpers for MAX30102 and MPU6050.

inline void i2cInit() {
  static bool done = false;
  if (done) return;
  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(20);          // ms - don't hang if a sensor is missing
  done = true;
}

inline bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

inline bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;      // repeated start
  size_t got = Wire.requestFrom((uint16_t)addr, (size_t)len, true);
  if (got != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

inline bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t& v) {
  return i2cReadBytes(addr, reg, &v, 1);
}
