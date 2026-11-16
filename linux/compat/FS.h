// SPDX-License-Identifier: GPL-3.0-or-later
// Arduino's fs::FS and File over POSIX files under a root directory, so an
// FS on Linux looks like a mounted SD card. Paths are absolute within the
// root; ".." is refused.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs {

class File {
 public:
  File() = default;
  File(File&& o) noexcept;
  File& operator=(File&& o) noexcept;
  ~File();

  explicit operator bool() const;
  bool isDirectory() const;
  // The base name, as the ESP32 core gives it.
  const char* name() const { return name_.c_str(); }
  const char* path() const { return path_.c_str(); }
  size_t size() const;
  size_t read(uint8_t* buf, size_t n);
  size_t write(const uint8_t* buf, size_t n);
  size_t write(uint8_t b) { return write(&b, 1); }
  File openNextFile();
  void close();

 private:
  friend class FS;
  struct Dir;
  std::string full_;  // the host path
  std::string path_;  // the path within the FS
  std::string name_;
  FILE* f_ = nullptr;
  std::unique_ptr<Dir> dir_;
};

class FS {
 public:
  explicit FS(std::string root) : root_(std::move(root)) {}
  const std::string& root() const { return root_; }

  File open(const char* path, const char* mode = FILE_READ, bool create = false);
  bool exists(const char* path);
  bool mkdir(const char* path);
  bool remove(const char* path);
  bool rmdir(const char* path);

 private:
  bool hostPath(const char* path, std::string& out) const;
  std::string root_;
};

}  // namespace fs

using fs::File;
