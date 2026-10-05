#pragma once
#include "Arduino.h"

typedef int gpio_num_t;
enum esp_sleep_source_t { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_EXT0 = 2, ESP_SLEEP_WAKEUP_TIMER = 4 };
typedef esp_sleep_source_t esp_sleep_wakeup_cause_t;

inline esp_sleep_wakeup_cause_t g_sim_cause = ESP_SLEEP_WAKEUP_UNDEFINED;   // scripted
inline uint32_t g_sim_sleep_s = 0;
inline bool     g_sim_ext0 = false;

struct SimDeepSleep { uint32_t seconds; bool motionArmed; };               // thrown by esp_deep_sleep_start()

inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return g_sim_cause; }
inline void esp_sleep_enable_timer_wakeup(uint64_t us) { g_sim_sleep_s = (uint32_t)(us / 1000000ULL); }
inline void esp_sleep_enable_ext0_wakeup(gpio_num_t, int) { g_sim_ext0 = true; }
inline void esp_sleep_disable_wakeup_source(int) { g_sim_ext0 = false; }
#define ESP_SLEEP_WAKEUP_ALL 0
[[noreturn]] inline void esp_deep_sleep_start() { throw SimDeepSleep{g_sim_sleep_s, g_sim_ext0}; }
