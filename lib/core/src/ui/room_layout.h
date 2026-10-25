#pragma once

#include <cstdint>
#include <deque>
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
};

// One screen row of the room log. Every line of a message carries its id;
// divider lines have divider = true and an empty id.
struct DisplayLine {
  std::vector<Span> spans;
  std::string messageId;
  bool mentionTint = false;
  bool divider = false;
};

struct LayoutOptions {
  TextSize size = TextSize::Compact;
  int cols = 53;
  std::string selfUsername;
  const std::set<std::string>* muted = nullptr;     // usernames
  const std::set<std::string>* revealed = nullptr;  // spoiler message ids
  const Theme* theme = nullptr;                     // defaultTheme() when null
};

// Lays out messages (oldest first) into display lines no wider than o.cols,
// following the rendering rules in spec §6.
std::vector<DisplayLine> layoutRoom(const std::deque<CircMessage>& msgs, const LayoutOptions& o);

}  // namespace rc
