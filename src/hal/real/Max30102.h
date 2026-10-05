#pragma once
#include <Arduino.h>
#include "../IHeartRate.h"
#include "../../core/hr_algo.h"
#include "config.h"
#include "i2c_bus.h"

// MAX30102 in multi-LED mode with only slot 1 = IR, so each FIFO sample is
// 3 bytes (IR only). The red LED stays off. 100 Hz sensor rate, 4x on-chip
// averaging -> 25 Hz samples. Registers per the MAX30102 datasheet.
//
// NOT yet tested on hardware.
class Max30102 : public IHeartRate {
  static constexpr uint8_t ADDR = 0x57;
  enum : uint8_t {
    REG_INT_EN1 = 0x02, REG_INT_EN2 = 0x03,
    REG_FIFO_WR = 0x04, REG_FIFO_OVF = 0x05, REG_FIFO_RD = 0x06, REG_FIFO_DATA = 0x07,
    REG_FIFO_CFG = 0x08, REG_MODE = 0x09, REG_SPO2 = 0x0A,
    REG_LED1 = 0x0C, REG_LED2 = 0x0D, REG_SLOT12 = 0x11, REG_SLOT34 = 0x12,
    REG_PART_ID = 0xFF
  };

  int32_t  buf[Cfg::PPG_SAMPLES];
  uint32_t n = 0;

public:
  bool begin() override {
    n = 0;
    i2cInit();
    uint8_t id = 0;
    if (!i2cRead8(ADDR, REG_PART_ID, id) || id != 0x15) return false;

    i2cWrite8(ADDR, REG_MODE, 0x40);                       // soft reset
    for (int i = 0; i < 20; i++) {                         // wait for reset bit to clear
      delay(5);
      uint8_t m = 0xFF;
      if (i2cRead8(ADDR, REG_MODE, m) && !(m & 0x40)) break;
    }

    bool ok = true;
    ok &= i2cWrite8(ADDR, REG_INT_EN1, 0x00);              // no interrupts, we poll
    ok &= i2cWrite8(ADDR, REG_INT_EN2, 0x00);
    ok &= i2cWrite8(ADDR, REG_FIFO_CFG, 0x5F);             // avg x4, rollover on, almost-full 15
    ok &= i2cWrite8(ADDR, REG_SPO2, 0x27);                 // ADC 4096 nA, 100 Hz, 411 us (18-bit)
    ok &= i2cWrite8(ADDR, REG_LED1, 0x00);                 // red off
    ok &= i2cWrite8(ADDR, REG_LED2, Cfg::PPG_LED_PA);      // IR current
    ok &= i2cWrite8(ADDR, REG_SLOT12, 0x02);               // slot1 = LED2 (IR), slot2 off
    ok &= i2cWrite8(ADDR, REG_SLOT34, 0x00);
    ok &= i2cWrite8(ADDR, REG_FIFO_WR, 0x00);              // clear FIFO pointers
    ok &= i2cWrite8(ADDR, REG_FIFO_OVF, 0x00);
    ok &= i2cWrite8(ADDR, REG_FIFO_RD, 0x00);
    ok &= i2cWrite8(ADDR, REG_MODE, 0x07);                 // multi-LED mode: starts sampling
    return ok;
  }

  void poll() override {
    uint8_t wr, rd, ovf;
    if (!i2cRead8(ADDR, REG_FIFO_WR, wr) || !i2cRead8(ADDR, REG_FIFO_RD, rd)) return;
    int avail = (wr - rd) & 0x1F;
    if (avail == 0) {
      if (i2cRead8(ADDR, REG_FIFO_OVF, ovf) && ovf > 0) avail = 32;   // FIFO was full
      else return;
    }
    uint8_t raw[32 * 3];
    if (!i2cReadBytes(ADDR, REG_FIFO_DATA, raw, (size_t)avail * 3)) return;
    for (int i = 0; i < avail; i++) {
      int32_t v = ((int32_t)raw[i * 3] << 16) | ((int32_t)raw[i * 3 + 1] << 8) | raw[i * 3 + 2];
      v &= 0x3FFFF;                                        // 18-bit
      if (n < Cfg::PPG_SAMPLES) buf[n++] = v;
    }
  }

  HrResult finish() override {
    i2cWrite8(ADDR, REG_MODE, 0x80);                       // shutdown (~0.7 uA)
    uint32_t skip = Cfg::PPG_SKIP_SEC * Cfg::PPG_HZ;       // LED warm-up
    if (n <= skip) return HrResult{0, HrQuality::NONE};
    return computeHr(buf + skip, (int)(n - skip), (float)Cfg::PPG_HZ, Cfg::PPG_MIN_DC);
  }
};
