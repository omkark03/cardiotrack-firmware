#include "queue.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>
#include "config.h"
#include "log.h"

// File layout: [magic "CTR1"][metaLen u32][ecgN u32][imuN u32] meta-bytes ecg-int16 imu-int16
// File name:   /q/<bootCount 5 digits>_<seq 6 digits>.rec   (sorts oldest-first)

namespace Queue {

static const char* DIR = "/q";

struct Header { char magic[4]; uint32_t metaLen, ecgN, imuN; };

static const char* baseName(const char* n) {
  const char* s = strrchr(n, '/');
  return s ? s + 1 : n;
}

// Counts .rec files and returns the oldest name (lexically smallest).
static uint32_t scan(char* oldest, size_t cap) {
  uint32_t n = 0;
  if (oldest && cap) oldest[0] = 0;
  File dir = LittleFS.open(DIR);
  if (!dir || !dir.isDirectory()) return 0;
  File f = dir.openNextFile();
  while (f) {
    const char* nm = baseName(f.name());
    if (!f.isDirectory() && strstr(nm, ".rec")) {
      n++;
      if (oldest && cap && (oldest[0] == 0 || strcmp(nm, oldest) < 0)) {
        strncpy(oldest, nm, cap - 1);
        oldest[cap - 1] = 0;
      }
    }
    f = dir.openNextFile();
  }
  return n;
}

bool begin() {
  if (!LittleFS.begin(true)) { LOG("queue: LittleFS mount failed"); return false; }
  if (!LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
  return true;
}

uint32_t count() { return scan(nullptr, 0); }

static void pathFor(char* out, size_t cap, const char* name) { snprintf(out, cap, "%s/%s", DIR, name); }

static bool dropOldest() {
  char name[48], path[64];
  if (scan(name, sizeof name) == 0) return false;
  pathFor(path, sizeof path, name);
  LOG("queue: full, dropping oldest %s", name);
  return LittleFS.remove(path);
}

bool save(const BurstRecord& r, uint32_t bootCount, uint32_t seq) {
  size_t need = sizeof(Header) + r.metaLen + (size_t)r.ecgN * 2 + (size_t)r.imuN * 2;

  // make room: limit on record count and on free flash
  while (count() >= Cfg::QUEUE_MAX_RECORDS ||
         (LittleFS.totalBytes() - LittleFS.usedBytes()) < need + Cfg::QUEUE_FREE_RESERVE) {
    if (!dropOldest()) { LOG("queue: no room and nothing to drop"); return false; }
  }

  char name[48], path[64];
  snprintf(name, sizeof name, "%05lu_%06lu.rec", (unsigned long)bootCount, (unsigned long)seq);
  pathFor(path, sizeof path, name);

  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) { LOG("queue: cannot create %s", path); return false; }
  Header h; memcpy(h.magic, "CTR1", 4); h.metaLen = r.metaLen; h.ecgN = r.ecgN; h.imuN = r.imuN;
  bool ok = f.write((const uint8_t*)&h, sizeof h) == sizeof h;
  ok = ok && f.write((const uint8_t*)r.meta, r.metaLen) == r.metaLen;
  ok = ok && f.write((const uint8_t*)r.ecg, (size_t)r.ecgN * 2) == (size_t)r.ecgN * 2;
  ok = ok && f.write((const uint8_t*)r.imu, (size_t)r.imuN * 2) == (size_t)r.imuN * 2;
  f.close();
  if (!ok) { LittleFS.remove(path); LOG("queue: write failed, removed %s", path); return false; }
  LOG("queue: saved %s (%u bytes)", name, (unsigned)need);
  return true;
}

bool loadOldest(BurstRecord& r, char* pathOut, size_t cap) {
  for (;;) {                                           // loop so corrupt files get skipped
    char name[48];
    if (scan(name, sizeof name) == 0) return false;
    snprintf(pathOut, cap, "%s/%s", DIR, name);

    File f = LittleFS.open(pathOut, FILE_READ);
    Header h;
    bool ok = f && f.read((uint8_t*)&h, sizeof h) == sizeof h && memcmp(h.magic, "CTR1", 4) == 0 &&
              h.metaLen > 0 && h.metaLen < sizeof(r.meta) &&
              h.ecgN <= Cfg::ECG_SAMPLES && h.imuN <= Cfg::IMU_SAMPLES;
    if (ok) {
      r.metaLen = h.metaLen; r.ecgN = h.ecgN; r.imuN = h.imuN;
      ok = f.read((uint8_t*)r.meta, h.metaLen) == h.metaLen &&
           f.read((uint8_t*)r.ecg, (size_t)h.ecgN * 2) == (size_t)h.ecgN * 2 &&
           f.read((uint8_t*)r.imu, (size_t)h.imuN * 2) == (size_t)h.imuN * 2;
      r.meta[r.metaLen] = 0;
    }
    if (f) f.close();
    if (ok) return true;
    LOG("queue: corrupt record %s, deleting", pathOut);
    LittleFS.remove(pathOut);
  }
}

void remove(const char* path) { LittleFS.remove(path); }

}  // namespace Queue
