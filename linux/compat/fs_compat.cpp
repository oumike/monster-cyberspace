// SPDX-License-Identifier: GPL-3.0-or-later
#include <FS.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

namespace fs {

struct File::Dir {
  DIR* d = nullptr;
  ~Dir() {
    if (d) closedir(d);
  }
};

File::File(File&& o) noexcept { *this = std::move(o); }
File::~File() { close(); }

File& File::operator=(File&& o) noexcept {
  if (this != &o) {
    close();
    full_ = std::move(o.full_);
    path_ = std::move(o.path_);
    name_ = std::move(o.name_);
    f_ = o.f_;
    o.f_ = nullptr;
    dir_ = std::move(o.dir_);
  }
  return *this;
}

File::operator bool() const { return f_ || dir_; }

bool File::isDirectory() const { return bool(dir_); }

size_t File::size() const {
  struct stat st {};
  if (!*this || ::stat(full_.c_str(), &st) != 0) return 0;
  return dir_ ? 0 : size_t(st.st_size);
}

size_t File::read(uint8_t* buf, size_t n) { return f_ ? std::fread(buf, 1, n, f_) : 0; }

size_t File::write(const uint8_t* buf, size_t n) { return f_ ? std::fwrite(buf, 1, n, f_) : 0; }

File File::openNextFile() {
  File out;
  if (!dir_ || !dir_->d) return out;
  while (dirent* e = readdir(dir_->d)) {
    if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
    const std::string full = full_ + "/" + e->d_name;
    struct stat st {};
    if (::stat(full.c_str(), &st) != 0) continue;
    out.full_ = full;
    out.path_ = (path_ == "/" ? "" : path_) + "/" + e->d_name;
    out.name_ = e->d_name;
    if (S_ISDIR(st.st_mode)) {
      out.dir_ = std::make_unique<Dir>();
      out.dir_->d = opendir(full.c_str());
      if (!out.dir_->d) out.dir_.reset();
    } else {
      out.f_ = std::fopen(full.c_str(), "rb");
    }
    if (out) return out;
  }
  return out;
}

void File::close() {
  if (f_) std::fclose(f_);
  f_ = nullptr;
  dir_.reset();
}

bool FS::hostPath(const char* path, std::string& out) const {
  if (!path || path[0] != '/') return false;
  std::string p(path);
  if (!mount_.empty()) {
    if (p.compare(0, mount_.size(), mount_) != 0) return false;
    p.erase(0, mount_.size());
    if (p.empty()) p = "/";
    if (p[0] != '/') return false;
  }
  for (size_t i = 0; i < p.size();) {
    const size_t j = p.find('/', i + 1);
    const std::string part = p.substr(i + 1, (j == std::string::npos ? p.size() : j) - i - 1);
    if (part == "..") return false;
    if (j == std::string::npos) break;
    i = j;
  }
  out = root_ + (p == "/" ? "" : p);
  return true;
}

File FS::open(const char* path, const char* mode, bool) {
  File f;
  std::string full;
  if (!hostPath(path, full)) return f;
  f.full_ = full;
  f.path_ = path;
  const char* slash = std::strrchr(path, '/');
  f.name_ = slash ? slash + 1 : path;
  struct stat st {};
  const bool reading = !mode || mode[0] == 'r';
  if (reading && ::stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    f.dir_ = std::make_unique<File::Dir>();
    f.dir_->d = opendir(full.c_str());
    if (!f.dir_->d) f.dir_.reset();
    return f;
  }
  const char* m = reading ? "rb" : mode[0] == 'a' ? "ab" : "wb";
  if (reading) {
    f.f_ = std::fopen(full.c_str(), m);
  } else {
    // New files are 0600, since backups hold passwords.
    const int fd = ::open(full.c_str(), O_WRONLY | O_CREAT | (mode[0] == 'a' ? O_APPEND : O_TRUNC),
                          0600);
    if (fd >= 0 && !(f.f_ = ::fdopen(fd, m))) ::close(fd);
  }
  return f;
}

bool FS::exists(const char* path) {
  std::string full;
  struct stat st {};
  return hostPath(path, full) && ::stat(full.c_str(), &st) == 0;
}

bool FS::mkdir(const char* path) {
  std::string full;
  return hostPath(path, full) && ::mkdir(full.c_str(), 0700) == 0;
}

bool FS::remove(const char* path) {
  std::string full;
  return hostPath(path, full) && ::unlink(full.c_str()) == 0;
}

bool FS::rmdir(const char* path) {
  std::string full;
  return hostPath(path, full) && ::rmdir(full.c_str()) == 0;
}

}  // namespace fs
