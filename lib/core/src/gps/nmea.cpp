#include "gps/nmea.h"

#include <cstring>

#include "time/civil.h"

namespace rc {
namespace {

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Parses `n` digits at s; false if any is not a digit.
bool digits(const char* s, int n, int& out) {
  out = 0;
  for (int i = 0; i < n; ++i) {
    if (!isDigit(s[i])) return false;
    out = out * 10 + (s[i] - '0');
  }
  return true;
}

// A non-negative integer field; false if empty or not all digits.
bool parseInt(const char* s, int& out) {
  if (!*s) return false;
  out = 0;
  for (; *s; ++s) {
    if (!isDigit(*s)) return false;
    if (out > 100000) return false;
    out = out * 10 + (*s - '0');
  }
  return true;
}

// "ddmm.mmmm" (degDigits 2) or "dddmm.mmmm" (degDigits 3) as degrees.
bool parseCoord(const char* s, int degDigits, double& out) {
  int deg;
  if (!digits(s, degDigits, deg)) return false;
  s += degDigits;
  int whole;
  if (!digits(s, 2, whole)) return false;
  s += 2;
  double minutes = whole;
  if (*s == '.') {
    double scale = 0.1;
    for (++s; isDigit(*s); ++s, scale /= 10) minutes += (*s - '0') * scale;
  }
  if (*s || minutes >= 60) return false;
  out = deg + minutes / 60.0;
  return true;
}

// "hhmmss[.fff]" as ms since midnight.
bool parseTime(const char* s, int64_t& out) {
  int h, m, sec;
  if (!digits(s, 2, h) || !digits(s + 2, 2, m) || !digits(s + 4, 2, sec)) return false;
  if (h > 23 || m > 59 || sec > 60) return false;
  s += 6;
  int ms = 0;
  if (*s == '.') {
    int scale = 100;
    for (++s; isDigit(*s); ++s, scale /= 10) ms += (*s - '0') * scale;
  }
  if (*s) return false;
  out = ((h * 60 + m) * 60 + sec) * 1000LL + ms;
  return true;
}

// "ddmmyy" as days since 1970-01-01, year 2000 + yy.
bool parseDate(const char* s, int64_t& out) {
  int d, m, y;
  if (strlen(s) != 6 || !digits(s, 2, d) || !digits(s + 2, 2, m) || !digits(s + 4, 2, y))
    return false;
  if (d < 1 || d > 31 || m < 1 || m > 12) return false;
  out = daysFromCivil(2000 + y, m, d);
  return true;
}

}  // namespace

void NmeaParser::reset() {
  state_ = GpsState{};
  nTalkers_ = 0;
  len_ = 0;
  inSentence_ = false;
}

void NmeaParser::feed(const uint8_t* data, size_t n, int64_t nowMs) {
  for (size_t i = 0; i < n; ++i) {
    const char c = static_cast<char>(data[i]);
    if (c == '$') {
      inSentence_ = true;
      len_ = 0;
      continue;
    }
    if (!inSentence_) continue;
    if (c == '\n') {
      if (len_ && buf_[len_ - 1] == '\r') --len_;
      buf_[len_] = '\0';
      inSentence_ = false;
      handleLine(nowMs);
      continue;
    }
    // '$' + body + "\r\n" may not exceed kMaxSentence; the body here may still
    // hold its trailing '\r'.
    if (len_ + 1 > kMaxSentence - 2) {
      inSentence_ = false;  // too long: wait for the next '$'
      continue;
    }
    buf_[len_++] = c;
  }
}

void NmeaParser::handleLine(int64_t nowMs) {
  char* star = strchr(buf_, '*');
  if (!star || strlen(star) != 3) return;
  const int hi = hexVal(star[1]), lo = hexVal(star[2]);
  if (hi < 0 || lo < 0) return;
  uint8_t x = 0;
  for (const char* p = buf_; p < star; ++p) x ^= static_cast<uint8_t>(*p);
  if (x != ((hi << 4) | lo)) return;
  *star = '\0';

  constexpr int kMaxFields = 24;
  char* f[kMaxFields];
  int nf = 0;
  f[nf++] = buf_;
  for (char* p = buf_; *p; ++p) {
    if (*p == ',') {
      *p = '\0';
      if (nf == kMaxFields) break;
      f[nf++] = p + 1;
    }
  }

  if (strlen(f[0]) != 5) return;  // 2-letter talker + 3-letter type
  const char* type = f[0] + 2;
  if (strcmp(type, "RMC") == 0) handleRmc(f, nf, nowMs);
  else if (strcmp(type, "GGA") == 0) handleGga(f, nf);
  else if (strcmp(type, "GSV") == 0) handleGsv(f[0], f, nf);
}

void NmeaParser::handleRmc(char** f, int nf, int64_t nowMs) {
  if (nf < 10) return;
  if (strcmp(f[2], "V") == 0) {
    state_.fix = false;
    return;
  }
  if (strcmp(f[2], "A") != 0) return;
  state_.fix = true;
  state_.lastFixMs = nowMs;

  double lat, lon;
  if (parseCoord(f[3], 2, lat) && parseCoord(f[5], 3, lon) &&
      (f[4][0] == 'N' || f[4][0] == 'S') && (f[6][0] == 'E' || f[6][0] == 'W')) {
    state_.lat = f[4][0] == 'S' ? -lat : lat;
    state_.lon = f[6][0] == 'W' ? -lon : lon;
    state_.posAtMs = nowMs;
  }

  int64_t timeMs, days;
  if (parseTime(f[1], timeMs) && parseDate(f[9], days)) {
    state_.utcValid = true;
    state_.utcEpochMs = days * 86400000LL + timeMs;
    state_.utcAtMs = nowMs;
  }
}

void NmeaParser::handleGga(char** f, int nf) {
  if (nf < 8) return;
  int sats;
  state_.satsUsed = parseInt(f[7], sats) ? sats : 0;
}

void NmeaParser::handleGsv(const char* talker, char** f, int nf) {
  if (nf < 4) return;
  int msg, inView;
  if (!parseInt(f[2], msg) || msg != 1 || !parseInt(f[3], inView)) return;

  size_t i = 0;
  while (i < nTalkers_ && memcmp(talkers_[i].id, talker, 2) != 0) ++i;
  if (i == nTalkers_) {
    if (nTalkers_ == kMaxTalkers) return;
    memcpy(talkers_[i].id, talker, 2);
    ++nTalkers_;
  }
  talkers_[i].count = inView;

  int sum = 0;
  for (size_t j = 0; j < nTalkers_; ++j) sum += talkers_[j].count;
  state_.satsInView = sum;
}

}  // namespace rc
