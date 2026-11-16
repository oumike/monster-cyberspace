// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <climits>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/sysinfo.h>
#endif

namespace {

const std::chrono::steady_clock::time_point kStart = std::chrono::steady_clock::now();

std::mutex rngLock;
std::mt19937& rng() {
  static std::mt19937 r{std::random_device{}()};
  return r;
}

std::vector<std::string> args;

}  // namespace

int64_t esp_timer_get_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - kStart)
      .count();
}

uint32_t millis() { return uint32_t(esp_timer_get_time() / 1000); }
uint32_t micros() { return uint32_t(esp_timer_get_time()); }
void delay(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void yield() { std::this_thread::yield(); }

long random(long max) { return random(0, max); }

long random(long min, long max) {
  if (max <= min) return min;
  std::lock_guard<std::mutex> g(rngLock);
  return std::uniform_int_distribution<long>(min, max - 1)(rng());
}

uint32_t esp_random() {
  static std::random_device dev;  // the kernel's generator
  std::lock_guard<std::mutex> g(rngLock);
  return dev();
}

// ---- Serial ----

HardwareSerial Serial;

int HardwareSerial::printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const int n = std::vprintf(fmt, ap);
  va_end(ap);
  std::fflush(stdout);
  return n;
}

size_t HardwareSerial::print(const char* s) {
  const size_t n = std::fwrite(s, 1, std::strlen(s), stdout);
  std::fflush(stdout);
  return n;
}

size_t HardwareSerial::println(const char* s) {
  const size_t n = print(s);
  return n + print("\r\n");
}

size_t HardwareSerial::write(uint8_t b) { return write(&b, 1); }

size_t HardwareSerial::write(const uint8_t* buf, size_t n) {
  const size_t w = std::fwrite(buf, 1, n, stdout);
  std::fflush(stdout);
  return w;
}

int HardwareSerial::available() {
  if (eof_) return 0;
  pollfd p{STDIN_FILENO, POLLIN, 0};
  if (poll(&p, 1, 0) <= 0 || !(p.revents & (POLLIN | POLLHUP))) return 0;
  int n = 0;
  if (ioctl(STDIN_FILENO, FIONREAD, &n) == 0 && n > 0) return n;
  // Readable with nothing queued: end of input. read() finds out.
  return 1;
}

int HardwareSerial::read() {
  if (eof_) return -1;
  uint8_t c;
  const ssize_t r = ::read(STDIN_FILENO, &c, 1);
  if (r == 1) return c;
  if (r == 0) eof_ = true;  // /dev/null or a closed pipe: stop polling it
  return -1;
}

void HardwareSerial::flush() { std::fflush(stdout); }

// ---- ESP ----

EspClass ESP;

void compat::setArgs(int argc, char** argv) { args.assign(argv, argv + argc); }

void EspClass::restart() {
  std::fflush(stdout);
  std::fflush(stderr);
  if (!args.empty()) {
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
#if defined(__linux__)
    execv("/proc/self/exe", argv.data());
#endif
    execvp(argv[0], argv.data());
  }
  std::perror("ESP.restart");
  std::_Exit(1);
}

// ---- heap_caps ----

void* heap_caps_malloc(size_t size, uint32_t) { return std::malloc(size); }
void* heap_caps_calloc(size_t n, size_t size, uint32_t) { return std::calloc(n, size); }
void* heap_caps_realloc(void* p, size_t size, uint32_t) { return std::realloc(p, size); }
void heap_caps_free(void* p) { std::free(p); }

size_t heap_caps_get_free_size(uint32_t) {
  size_t avail = size_t(256) << 20;  // where the system can't say
#if defined(__linux__)
  struct sysinfo si {};
  if (sysinfo(&si) == 0) avail = size_t(si.freeram + si.bufferram) * si.mem_unit;
#endif
  // Callers print it as unsigned; keep it in range.
  return avail > UINT_MAX ? UINT_MAX : avail;
}

size_t heap_caps_get_largest_free_block(uint32_t caps) { return heap_caps_get_free_size(caps); }
size_t heap_caps_get_minimum_free_size(uint32_t caps) { return heap_caps_get_free_size(caps); }
