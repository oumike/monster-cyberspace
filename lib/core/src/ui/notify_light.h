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

// What the light tells about, in the order a round shows them.
enum class NotifySource : uint8_t { Channel, Mention, Feed, Cmail };
constexpr int kNotifySourceCount = 4;

// Each source's colour (Off leaves it out) and whether it still has
// something unread.
struct NotifyInputs {
  LedColor color[kNotifySourceCount] = {};
  bool unread[kNotifySourceCount] = {};
};

// The blink pattern as a pure state machine. Each pending source gets a
// double blink in its colour, one after another, then a pause; the round
// repeats each second at most. tick gives the rgb bits to show now; the
// caller writes the light only when they change. A round under way always
// finishes, dark. Each source stops when read, or a timeout after its last
// message.
class NotifyBlinker {
 public:
  void message(NotifySource s, uint32_t nowMs);  // a new one arrived
  void preview(LedColor c, uint32_t nowMs);      // one double blink in c
  void cancel();                                 // dark at once, disarmed
  uint8_t tick(uint32_t nowMs, const NotifyInputs& in, uint16_t timeoutSecs);
  // Anything left to show: the caller needs the inputs only while this holds.
  bool active() const;

 private:
  void startBlink(LedColor c, uint32_t atMs);
  bool startRound(uint32_t nowMs, const NotifyInputs& in, uint16_t timeoutSecs);

  struct Src {
    bool armed = false;
    bool kick = false;  // in the next round, unread or not
    uint32_t armedMs = 0;
  };
  Src src_[kNotifySourceCount];
  LedColor queue_[kNotifySourceCount] = {};
  uint8_t queueLen_ = 0, queuePos_ = 0;
  uint8_t phasesLeft_ = 0;  // on and off halves left in this blink
  bool running_ = false;    // a round or a preview
  bool lit_ = false;
  bool ended_ = false;      // endMs_ and dueMs_ hold
  uint32_t roundMs_ = 0, nextMs_ = 0, endMs_ = 0, dueMs_ = 0;
};

}  // namespace rc
