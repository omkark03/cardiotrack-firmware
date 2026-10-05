#include "hr_algo.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

const char* hrQualityName(HrQuality q) {
  switch (q) {
    case HrQuality::GOOD: return "good";
    case HrQuality::POOR: return "poor";
    default:              return "none";
  }
}

static int cmpFloat(const void* a, const void* b) {
  float x = *(const float*)a, y = *(const float*)b;
  return (x > y) - (x < y);
}

// Peak-interval heart rate from a reflective PPG trace.
// Steps: DC check -> remove slow baseline (1 s moving average) -> light smoothing
//        -> local maxima above 0.5 sigma, at least 0.33 s apart -> median interval.
HrResult computeHr(const int32_t* ir, int n, float fs, int32_t minDc) {
  HrResult res{0, HrQuality::NONE};
  if (n < (int)(fs * 8.0f)) return res;                       // need >= 8 s of data

  double sum = 0;
  for (int i = 0; i < n; i++) sum += ir[i];
  double dc = sum / n;
  if (dc < (double)minDc) return res;                         // no skin contact

  const int w = (int)fs | 1;                                  // ~1 s window, odd
  const int half = w / 2;
  float* x = (float*)malloc(sizeof(float) * n);
  if (!x) return res;

  // prefix sums for the moving average
  double* ps = (double*)malloc(sizeof(double) * (n + 1));
  if (!ps) { free(x); return res; }
  ps[0] = 0;
  for (int i = 0; i < n; i++) ps[i + 1] = ps[i] + ir[i];

  // detrend. Reflective PPG gets darker with each pulse, so invert: pulses become peaks.
  for (int i = 0; i < n; i++) {
    int a = i - half < 0 ? 0 : i - half;
    int b = i + half + 1 > n ? n : i + half + 1;
    double ma = (ps[b] - ps[a]) / (b - a);
    x[i] = (float)(ma - ir[i]);
  }
  free(ps);

  // 3-point smoothing, in place (use a running previous value)
  float prev = x[0];
  for (int i = 1; i < n - 1; i++) {
    float cur = x[i];
    x[i] = (prev + cur + x[i + 1]) / 3.0f;
    prev = cur;
  }

  double ss = 0;
  for (int i = 0; i < n; i++) ss += (double)x[i] * x[i];
  float sigma = (float)sqrt(ss / n);
  if (sigma < 1.0f) { free(x); return res; }                  // flat signal

  const int minDist = (int)(0.33f * fs) > 1 ? (int)(0.33f * fs) : 1;
  int* peaks = (int*)malloc(sizeof(int) * (n / 2 + 2));
  if (!peaks) { free(x); return res; }
  int np = 0;
  for (int i = 1; i < n - 1; i++) {
    if (x[i] > 0.5f * sigma && x[i] > x[i - 1] && x[i] >= x[i + 1]) {
      if (np > 0 && i - peaks[np - 1] < minDist) {
        if (x[i] > x[peaks[np - 1]]) peaks[np - 1] = i;       // keep the taller one
      } else {
        peaks[np++] = i;
      }
    }
  }

  // Refine each peak to sub-sample accuracy with a parabola through its 3 points.
  // (At 25 Hz a whole-sample peak gives +-4 bpm error at 110 bpm; this removes most of it.)
  float* pos = (float*)malloc(sizeof(float) * (np + 1));
  if (pos) {
    for (int k = 0; k < np; k++) {
      int p = peaks[k];
      float denom = x[p - 1] - 2.0f * x[p] + x[p + 1];
      float off = denom != 0.0f ? 0.5f * (x[p - 1] - x[p + 1]) / denom : 0.0f;
      if (off > 0.5f) off = 0.5f;
      if (off < -0.5f) off = -0.5f;
      pos[k] = (float)p + off;
    }
  }

  float* iv = (float*)malloc(sizeof(float) * (np + 1));
  int nv = 0;
  if (iv && pos) {
    for (int i = 1; i < np; i++) {
      float s = (pos[i] - pos[i - 1]) / fs;
      if (s >= 0.33f && s <= 1.5f) iv[nv++] = s;              // 40..180 bpm
    }
  }
  free(pos);
  free(peaks);
  free(x);

  if (!iv || nv < 3) { free(iv); return res; }

  qsort(iv, nv, sizeof(float), cmpFloat);
  float med = (nv & 1) ? iv[nv / 2] : 0.5f * (iv[nv / 2 - 1] + iv[nv / 2]);

  // spread: median absolute deviation of the intervals, relative to the median
  float* dev = (float*)malloc(sizeof(float) * nv);
  float mad = 1.0f;
  if (dev) {
    for (int i = 0; i < nv; i++) dev[i] = fabsf(iv[i] - med);
    qsort(dev, nv, sizeof(float), cmpFloat);
    mad = (nv & 1) ? dev[nv / 2] : 0.5f * (dev[nv / 2 - 1] + dev[nv / 2]);
    free(dev);
  }
  free(iv);

  res.bpm = (int)(60.0f / med + 0.5f);
  float expected = (n / fs) / med;                            // beats we should have seen
  bool regular = (mad / med) < 0.15f;
  bool enough  = nv >= 0.6f * expected && nv >= 8;
  res.quality = (regular && enough) ? HrQuality::GOOD : HrQuality::POOR;
  return res;
}
