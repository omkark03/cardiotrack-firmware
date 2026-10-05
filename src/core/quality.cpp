#include "quality.h"
#include "config.h"
#include <math.h>

EcgQuality ecgQuality(const int16_t* s, uint32_t n) {
  EcgQuality q{0.0f, 0.0f, false};
  if (n == 0) return q;
  uint32_t sat = 0;
  double sum = 0, sum2 = 0;
  for (uint32_t i = 0; i < n; i++) {
    int v = s[i];
    if (v <= Cfg::ECG_SAT_LOW || v >= Cfg::ECG_SAT_HIGH) sat++;
    sum += v;
    sum2 += (double)v * v;
  }
  double mean = sum / n;
  double var = sum2 / n - mean * mean;
  q.satPct = 100.0f * (float)sat / (float)n;
  q.stdCounts = var > 0 ? (float)sqrt(var) : 0.0f;
  q.ok = (q.satPct <= Cfg::ECG_SAT_MAX_PCT) && (q.stdCounts >= Cfg::ECG_FLAT_STD);
  return q;
}

const char* activityName(Activity a) {
  switch (a) {
    case Activity::REST:  return "resting";
    case Activity::LIGHT: return "light";
    default:              return "active";
  }
}

ImuStats imuStats(const int16_t* magMg, uint32_t n) {
  ImuStats st{0.0f, 0.0f, Activity::REST};
  if (n == 0) return st;
  double sum = 0;
  for (uint32_t i = 0; i < n; i++) sum += magMg[i];
  double mean = sum / n;
  double ss = 0;
  for (uint32_t i = 0; i < n; i++) { double d = magMg[i] - mean; ss += d * d; }
  st.meanMagG = (float)(mean / 1000.0);
  st.rmsDynG = (float)(sqrt(ss / n) / 1000.0);
  if (st.rmsDynG < Cfg::ACT_REST_G)       st.activity = Activity::REST;
  else if (st.rmsDynG < Cfg::ACT_LIGHT_G) st.activity = Activity::LIGHT;
  else                                    st.activity = Activity::ACTIVE;
  return st;
}

// Rough CR2450 curve: flat near 2.9 V, falls off a cliff below ~2.7 V.
int batteryPercent(int mv) {
  if (mv < 0) return -1;
  static const int pts[][2] = { {3000,100}, {2900,85}, {2800,55}, {2700,25}, {2500,5}, {2000,0} };
  const int N = sizeof(pts) / sizeof(pts[0]);
  if (mv >= pts[0][0]) return 100;
  for (int i = 1; i < N; i++) {
    if (mv >= pts[i][0]) {
      int dx = pts[i - 1][0] - pts[i][0];
      int dy = pts[i - 1][1] - pts[i][1];
      return pts[i][1] + (mv - pts[i][0]) * dy / dx;
    }
  }
  return 0;
}
