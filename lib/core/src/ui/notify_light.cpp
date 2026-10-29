#include "ui/notify_light.h"

namespace rc {

namespace {
constexpr uint8_t kRgb[kLedColorCount] = {0, 1, 2, 4, 3, 6, 5, 7};
constexpr const char* kKeys[kLedColorCount] = {"off",  "red",  "green",   "blue",
                                                "yellow", "cyan", "magenta", "white"};
constexpr const char32_t* kLabels[kLedColorCount] = {U"Off",    U"Red",  U"Green",   U"Blue",
                                                      U"Yellow", U"Cyan", U"Magenta", U"White"};
constexpr const char32_t* kTimeoutLabels[kNotifyTimeoutCount] = {U"15 s", U"30 s", U"1 min",
                                                                  U"5 min", U"Until read"};

// camillia-mt's pattern: two short flashes, repeated each second.
constexpr uint8_t kBlinks = 2;
constexpr uint32_t kOnMs = 60, kOffMs = 90, kRepeatMs = 1000;

int idx(LedColor c) {
  const int i = static_cast<int>(c);
  return i >= 0 && i < kLedColorCount ? i : 0;
}
}  // namespace

uint8_t ledColorRgb(LedColor c) { return kRgb[idx(c)]; }
const char* ledColorKey(LedColor c) { return kKeys[idx(c)]; }
const char32_t* ledColorLabel(LedColor c) { return kLabels[idx(c)]; }

LedColor ledColorFromIndex(int i, LedColor fallback) {
  return i >= 0 && i < kLedColorCount ? static_cast<LedColor>(i) : fallback;
}

bool parseLedColor(std::string_view key, LedColor& out) {
  for (int i = 0; i < kLedColorCount; ++i)
    if (key == kKeys[i]) {
      out = static_cast<LedColor>(i);
      return true;
    }
  return false;
}

bool validNotifyTimeout(uint32_t secs) {
  for (uint16_t s : kNotifyTimeoutSecs)
    if (s == secs) return true;
  return false;
}

uint16_t snapNotifyTimeout(uint32_t secs) {
  if (secs == 0) return 0;
  uint16_t longest = 0;
  for (uint16_t s : kNotifyTimeoutSecs) {
    if (s && s >= secs) return s;  // listed in ascending order before the 0
    if (s > longest) longest = s;
  }
  return longest;
}

const char32_t* notifyTimeoutLabel(uint32_t secs) {
  const uint16_t s = snapNotifyTimeout(secs);
  for (int i = 0; i < kNotifyTimeoutCount; ++i)
    if (kNotifyTimeoutSecs[i] == s) return kTimeoutLabels[i];
  return kTimeoutLabels[0];
}

void NotifyBlinker::start(LedColor c, uint32_t nowMs) {
  color_ = c;
  phasesLeft_ = kBlinks * 2;
  lit_ = false;
  patternMs_ = nowMs;
  nextMs_ = nowMs;
}

void NotifyBlinker::message(uint32_t nowMs) {
  armed_ = true;
  armedMs_ = nowMs;
  if (!phasesLeft_) kick_ = true;
}

void NotifyBlinker::preview(LedColor c, uint32_t nowMs) {
  lit_ = false;
  phasesLeft_ = 0;
  preview_ = false;
  kick_ = false;
  if (c == LedColor::Off) return;
  preview_ = true;
  start(c, nowMs);
}

void NotifyBlinker::cancel() {
  lit_ = false;
  phasesLeft_ = 0;
  preview_ = false;
  armed_ = false;
  kick_ = false;
}

uint8_t NotifyBlinker::tick(uint32_t nowMs, LedColor color, uint16_t timeoutSecs, bool unread) {
  if (!preview_ && color == LedColor::Off) {
    cancel();
    return 0;
  }
  if (!phasesLeft_) {
    if (preview_) {
      preview_ = false;
      return 0;
    }
    if (!armed_) return 0;
    if (timeoutSecs && nowMs - armedMs_ >= uint32_t(timeoutSecs) * 1000u) {
      armed_ = kick_ = false;
      return 0;
    }
    if (kick_) {
      kick_ = false;
      start(color, nowMs);
    } else if (!unread) {
      armed_ = false;
      return 0;
    } else if (nowMs - patternMs_ < kRepeatMs) {
      return 0;
    } else {
      start(color, nowMs);
    }
  }
  if (static_cast<int32_t>(nowMs - nextMs_) >= 0) {
    lit_ = !lit_;
    nextMs_ = nowMs + (lit_ ? kOnMs : kOffMs);
    --phasesLeft_;
  }
  return lit_ ? ledColorRgb(color_) : 0;
}

}  // namespace rc
