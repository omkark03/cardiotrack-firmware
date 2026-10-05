#pragma once
#include "IEcg.h"
#include "IHeartRate.h"
#include "IImu.h"
#include "ITemp.h"
#include "IBattery.h"

// The only place firmware gets a sensor from. Mock vs real is decided in factory.cpp.
IEcg&       ecg();
IHeartRate& ppg();
IImu&       imu();
ITemp&      tempSensor();
IBattery&   battery();
