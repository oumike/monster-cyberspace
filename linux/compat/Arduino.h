// SPDX-License-Identifier: GPL-3.0-or-later
// The part of the Arduino-ESP32 core the app uses, over POSIX: clocks,
// random numbers, a minimal String, Serial on stdin/stdout and ESP.restart.
// Like the ESP core's, it brings in the FreeRTOS headers.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void yield();

// [0, max) and [min, max), as Arduino's.
long random(long max);
long random(long min, long max);
uint32_t esp_random();

// Just enough of Arduino's String for Preferences::getString().
class String {
 public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator!=(const String& o) const { return s_ != o.s_; }

 private:
  std::string s_;
};

// Serial: output to stdout, input from stdin without blocking.
class HardwareSerial {
 public:
  void begin(unsigned long) {}
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  size_t print(const char* s);
  size_t println(const char* s = "");
  size_t write(uint8_t b);
  size_t write(const uint8_t* buf, size_t n);
  int available();
  int read();
  void flush();

 private:
  bool eof_ = false;
};
extern HardwareSerial Serial;

class EspClass {
 public:
  // Starts the program again in place (execv of itself).
  [[noreturn]] void restart();
};
extern EspClass ESP;

namespace compat {
// main() hands over its arguments for ESP.restart().
void setArgs(int argc, char** argv);
}  // namespace compat
