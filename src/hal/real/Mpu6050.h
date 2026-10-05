#pragma once
#include <Arduino.h>
#include "../IImu.h"
#include "config.h"
#include "i2c_bus.h"

// MPU6050, accelerometer only (gyros in standby).
//  begin():          normal mode, +-2 g, 25 Hz output - used while sampling the burst
//  armMotionWake():  low-power cycle mode (~5 Hz wake, tens of uA) with the
//                    motion-detect interrupt on the INT pin (latched, active high)
//
// NOT yet tested on hardware. The wake-on-motion register recipe is from the
// datasheet/register map; verify the INT pin actually fires and clears.
class Mpu6050 : public IImu {
  static constexpr uint8_t ADDR = 0x68;   // AD0 low
  enum : uint8_t {
    REG_SMPLRT_DIV = 0x19, REG_CONFIG = 0x1A, REG_ACCEL_CFG = 0x1C,
    REG_MOT_THR = 0x1F, REG_MOT_DUR = 0x20,
    REG_INT_PIN_CFG = 0x37, REG_INT_ENABLE = 0x38, REG_INT_STATUS = 0x3A,
    REG_ACCEL_XOUT_H = 0x3B, REG_MOT_DETECT_CTRL = 0x69,
    REG_PWR1 = 0x6B, REG_PWR2 = 0x6C, REG_WHO_AM_I = 0x75
  };

public:
  bool begin() override {
    i2cInit();
    uint8_t who = 0;
    if (!i2cRead8(ADDR, REG_WHO_AM_I, who) || who == 0x00 || who == 0xFF) return false;

    bool ok = true;
    ok &= i2cWrite8(ADDR, REG_INT_ENABLE, 0x00);       // no interrupt while sampling
    ok &= i2cWrite8(ADDR, REG_PWR1, 0x00);             // wake, internal 8 MHz clock
    ok &= i2cWrite8(ADDR, REG_PWR2, 0x07);             // gyro axes in standby, accel on
    ok &= i2cWrite8(ADDR, REG_ACCEL_CFG, 0x00);        // +-2 g, no high-pass
    ok &= i2cWrite8(ADDR, REG_CONFIG, 0x03);           // DLPF ~44 Hz
    ok &= i2cWrite8(ADDR, REG_SMPLRT_DIV, 39);         // 1 kHz / 40 = 25 Hz
    delay(20);                                         // let the accel settle
    return ok;
  }

  bool readAccel(float& ax, float& ay, float& az) override {
    uint8_t r[6];
    if (!i2cReadBytes(ADDR, REG_ACCEL_XOUT_H, r, 6)) return false;
    ax = (int16_t)((r[0] << 8) | r[1]) / 16384.0f;
    ay = (int16_t)((r[2] << 8) | r[3]) / 16384.0f;
    az = (int16_t)((r[4] << 8) | r[5]) / 16384.0f;
    return true;
  }

  bool armMotionWake() override {
    i2cInit();
    uint8_t dummy;
    bool ok = true;
    ok &= i2cWrite8(ADDR, REG_PWR1, 0x00);                         // make sure awake while configuring
    ok &= i2cWrite8(ADDR, REG_ACCEL_CFG, 0x01);                    // +-2 g, 5 Hz high-pass (needed for motion detect)
    ok &= i2cWrite8(ADDR, REG_MOT_THR, Cfg::IMU_MOTION_THR);       // 2 mg per LSB
    ok &= i2cWrite8(ADDR, REG_MOT_DUR, 1);
    ok &= i2cWrite8(ADDR, REG_MOT_DETECT_CTRL, 0x15);
    ok &= i2cWrite8(ADDR, REG_INT_PIN_CFG, 0x30);                  // active high, push-pull, latched, cleared on status read
    ok &= i2cWrite8(ADDR, REG_INT_ENABLE, 0x40);                   // motion interrupt
    i2cRead8(ADDR, REG_INT_STATUS, dummy);                         // clear anything pending
    ok &= i2cWrite8(ADDR, REG_PWR2, (uint8_t)((Cfg::IMU_LP_WAKE << 6) | 0x07));  // wake rate + gyros standby
    ok &= i2cWrite8(ADDR, REG_PWR1, 0x28);                         // CYCLE mode, temp sensor off
    delay(5);
    i2cRead8(ADDR, REG_INT_STATUS, dummy);                         // INT pin must be low before sleeping
    return ok;
  }
};
