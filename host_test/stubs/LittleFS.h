#pragma once
#include "Arduino.h"

// LittleFS backed by a real folder (sim_flash/) so the queue code runs for real.
namespace fs {
namespace sim {
  inline std::string root = "sim_flash";
  inline std::filesystem::path real(const char* p) { return std::filesystem::path(root) / (p[0] == '/' ? p + 1 : p); }
}

class File {
  struct Impl { FILE* fp = nullptr; bool dir = false; std::string name; std::vector<std::string> entries; size_t idx = 0; std::string path; ~Impl() { if (fp) fclose(fp); } };
  std::shared_ptr<Impl> d;
public:
  File() {}
  explicit File(std::shared_ptr<Impl> i) : d(i) {}
  static File openFile(const char* p, const char* mode) {
    auto i = std::make_shared<Impl>(); i->path = p; i->name = p;
    i->fp = fopen(sim::real(p).c_str(), mode);
    return i->fp ? File(i) : File();
  }
  static File openDir(const char* p) {
    auto i = std::make_shared<Impl>(); i->dir = true; i->path = p; i->name = p;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(sim::real(p), ec)) i->entries.push_back(e.path().filename().string());
    return File(i);
  }
  operator bool() const { return (bool)d; }
  bool isDirectory() const { return d && d->dir; }
  const char* name() const { return d->name.c_str(); }
  File openNextFile() {
    if (!d || !d->dir || d->idx >= d->entries.size()) return File();
    std::string n = d->entries[d->idx++];
    auto i = std::make_shared<Impl>(); i->name = n; i->path = d->path + "/" + n;
    return File(i);          // name only: enough for the queue's directory scan
  }
  size_t write(const uint8_t* b, size_t n) { return d && d->fp ? fwrite(b, 1, n, d->fp) : 0; }
  size_t read(uint8_t* b, size_t n) { return d && d->fp ? fread(b, 1, n, d->fp) : 0; }
  void close() { if (d && d->fp) { fclose(d->fp); d->fp = nullptr; } }
};
}  // namespace fs
using fs::File;

inline const char* FILE_READ = "rb";
inline const char* FILE_WRITE = "wb";

class LittleFSClass {
public:
  bool begin(bool = false) { std::filesystem::create_directories(fs::sim::root); return true; }
  bool exists(const char* p) { return std::filesystem::exists(fs::sim::real(p)); }
  bool mkdir(const char* p) { return std::filesystem::create_directories(fs::sim::real(p)); }
  bool remove(const char* p) { return std::filesystem::remove(fs::sim::real(p)); }
  File open(const char* p, const char* mode = "rb") {
    if (strcmp(mode, "rb") == 0 && std::filesystem::is_directory(fs::sim::real(p))) return File::openDir(p);
    return File::openFile(p, mode);
  }
  size_t totalBytes() { return 1900000; }
  size_t usedBytes() {
    size_t t = 0; std::error_code ec;
    for (auto& e : std::filesystem::recursive_directory_iterator(fs::sim::root, ec))
      if (e.is_regular_file()) t += e.file_size();
    return t;
  }
};
inline LittleFSClass LittleFS;
