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
// Dark between two sources' blinks, and after a round's last.
constexpr uint32_t kGapMs = 300, kPauseMs = 700;

bool reached(uint32_t nowMs, uint32_t atMs) { return static_cast<int32_t>(nowMs - atMs) >= 0; }

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

void NotifyBlinker::startBlink(LedColor c, uint32_t atMs) {
  queue_[queuePos_] = c;
  phasesLeft_ = kBlinks * 2;
  lit_ = false;
  nextMs_ = atMs;
}

void NotifyBlinker::message(NotifySource s, uint32_t nowMs) {
  Src& r = src_[static_cast<int>(s)];
  r.armed = true;
  r.kick = true;
  r.armedMs = nowMs;
}

void NotifyBlinker::preview(LedColor c, uint32_t nowMs) {
  running_ = false;
  lit_ = false;
  phasesLeft_ = 0;
  if (c == LedColor::Off) return;
  queueLen_ = 1;
  queuePos_ = 0;
  running_ = true;
  roundMs_ = nowMs;
  startBlink(c, nowMs);
}

void NotifyBlinker::cancel() {
  running_ = lit_ = false;
  phasesLeft_ = 0;
  for (Src& r : src_) r = Src{};
}

bool NotifyBlinker::active() const {
  if (running_) return true;
  for (const Src& r : src_)
    if (r.armed) return true;
  return false;
}

bool NotifyBlinker::startRound(uint32_t nowMs, const NotifyInputs& in, uint16_t timeoutSecs) {
  bool kicked = false, any = false;
  for (int i = 0; i < kNotifySourceCount; ++i) {
    Src& r = src_[i];
    if (!r.armed) continue;
    if (in.color[i] == LedColor::Off ||
        (timeoutSecs && nowMs - r.armedMs >= uint32_t(timeoutSecs) * 1000u) ||
        (!r.kick && !in.unread[i])) {
      r = Src{};
      continue;
    }
    any = true;
    kicked |= r.kick;
  }
  if (!any) return false;
  // A new message starts a round soon; otherwise the round is due.
  if (ended_ && !reached(nowMs, kicked ? endMs_ + kGapMs : dueMs_)) return false;
  queueLen_ = queuePos_ = 0;
  for (int i = 0; i < kNotifySourceCount; ++i) {
    Src& r = src_[i];
    if (!r.armed) continue;
    r.kick = false;
    queue_[queueLen_++] = in.color[i];
  }
  running_ = true;
  roundMs_ = nowMs;
  startBlink(queue_[0], nowMs);
  return true;
}

uint8_t NotifyBlinker::tick(uint32_t nowMs, const NotifyInputs& in, uint16_t timeoutSecs) {
  if (!running_ && !startRound(nowMs, in, timeoutSecs)) return 0;
  if (phasesLeft_ && reached(nowMs, nextMs_)) {
    lit_ = !lit_;
    nextMs_ = nowMs + (lit_ ? kOnMs : kOffMs);
    if (--phasesLeft_ == 0) {
      if (++queuePos_ < queueLen_) {
        startBlink(queue_[queuePos_], nowMs + kGapMs);
      } else {
        running_ = false;
        ended_ = true;
        endMs_ = nowMs;
        const uint32_t a = roundMs_ + kRepeatMs, b = nowMs + kPauseMs;
        dueMs_ = reached(b, a) ? b : a;
      }
    }
  }
  return lit_ ? ledColorRgb(queue_[queuePos_ < queueLen_ ? queuePos_ : queueLen_ - 1]) : 0;
}

}  // namespace rc
