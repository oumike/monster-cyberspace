#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "chat/message.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

struct Span {
  std::u32string text;
  uint16_t fg;
  bool bold = false;
};

// One screen row of the room log. Every line of a message carries its id;
// divider lines have divider = true and an empty id.
struct DisplayLine {
  std::vector<Span> spans;
  std::string messageId;
  bool mentionTint = false;
  bool divider = false;
  // ChatStyle::Bubbles: the row is part of a bubble covering columns
  // [bubbleCol, bubbleCol + bubbleCols); bubbleTop / bubbleBottom on its first
  // and last rows (rounded corners).
  enum class Bubble : uint8_t { None, Other, Self };
  Bubble bubble = Bubble::None;
  uint8_t bubbleCol = 0, bubbleCols = 0;
  bool bubbleTop = false, bubbleBottom = false;
};

struct LayoutOptions {
  TextSize size = TextSize::Compact;
  int cols = 53;
  std::string selfUsername;
  const std::set<std::string>* muted = nullptr;     // lower-cased usernames
  const std::set<std::string>* revealed = nullptr;  // spoiler message ids
  const Theme* theme = nullptr;                     // defaultTheme() when null
  // Compact layout only: wrapped lines start under the nick rather than under
  // the text (the room log at Comfortable and Large).
  bool indentToNick = false;
  // Bubbles: a dim line with the nick and time, then the text in a bubble,
  // yours on the right (spec 2026-11-09-cyberspace-chat-style-design.md).
  ChatStyle style = ChatStyle::Irc;
};

// Lays out messages (oldest first) into display lines no wider than o.cols,
// following the rendering rules in spec §6.
std::vector<DisplayLine> layoutRoom(const std::deque<CircMessage>& msgs, const LayoutOptions& o);

// Lays out msgs[from, to) one line at a time, as layoutRoom's lines for those
// messages: a day divider comes first when the message shown before `from`
// is on another day. Nothing is kept, so a long room never sits in memory
// laid out.
void layoutRoomEach(const std::deque<CircMessage>& msgs, const LayoutOptions& o, size_t from,
                    size_t to, const std::function<void(DisplayLine&&)>& out);

}  // namespace rc
