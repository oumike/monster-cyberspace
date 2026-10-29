#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// The new-message light, after platy-aprs: a double blink every second while
// something is unread, for at most a timeout after the last message arrived.
// Boards with board::hasNotifyLight. The values are saved: never reorder.
enum class LedColor : uint8_t { Off, Red, Green, Blue, Yellow, Cyan, Magenta, White };
constexpr int kLedColorCount = 8;

uint8_t ledColorRgb(LedColor c);             // bit0 red, bit1 green, bit2 blue
const char* ledColorKey(LedColor c);         // "off", "red", ...
const char32_t* ledColorLabel(LedColor c);   // U"Off", U"Red", ...
// Out of range gives fallback.
LedColor ledColorFromIndex(int i, LedColor fallback = LedColor::Off);
bool parseLedColor(std::string_view key, LedColor& out);  // out unchanged on failure

// How long it keeps blinking after the last message; 0 is until read.
constexpr uint16_t kNotifyTimeoutSecs[] = {15, 30, 60, 300, 0};
constexpr int kNotifyTimeoutCount = 5;

bool validNotifyTimeout(uint32_t secs);
// A listed value stays; otherwise the next longer option, or the longest.
uint16_t snapNotifyTimeout(uint32_t secs);
const char32_t* notifyTimeoutLabel(uint32_t secs);  // of the snapped value

// The blink pattern as a pure state machine. tick gives the rgb bits to show
// now; the caller writes the light only when they change. A pattern under way
// always finishes, dark.
class NotifyBlinker {
 public:
  void message(uint32_t nowMs);                // a new message arrived
  void preview(LedColor c, uint32_t nowMs);    // one double blink in c
  void cancel();                               // dark at once, disarmed
  uint8_t tick(uint32_t nowMs, LedColor color, uint16_t timeoutSecs, bool unread);
  // Anything left to show: the caller needs `unread` only while this holds.
  bool active() const { return armed_ || phasesLeft_ || preview_; }

 private:
  void start(LedColor c, uint32_t nowMs);

  LedColor color_ = LedColor::Off;
  uint8_t phasesLeft_ = 0;  // on and off halves left in the pattern
  bool lit_ = false;
  bool preview_ = false;
  bool armed_ = false;
  bool kick_ = false;       // start a pattern on the next tick, unread or not
  uint32_t armedMs_ = 0, patternMs_ = 0, nextMs_ = 0;
};

}  // namespace rc
