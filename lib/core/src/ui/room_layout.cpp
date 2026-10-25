#include "ui/room_layout.h"

#include <algorithm>

#include "chat/mention.h"
#include "ui/nick_color.h"
#include "ui/timefmt.h"
#include "ui/wrap.h"
#include "util/base64.h"
#include "util/utf8.h"

namespace rc {

namespace {

constexpr int kCompactTimeCols = 6;   // "14:02 "
constexpr int kCompactIndentCap = 20;
constexpr int kComfortNickCols = 10;  // nick padded to 10, then a space
constexpr int kComfortBodyCol = kComfortNickCols + 1;

struct Block {
  std::u32string text;
  uint16_t fg;
};

bool hasStyle(const CircMessage& m, const char* s) {
  return std::find(m.styles.begin(), m.styles.end(), s) != m.styles.end();
}

int lineWidth(const DisplayLine& l) {
  int n = 0;
  for (const auto& s : l.spans) n += static_cast<int>(s.text.size());
  return n;
}

void clipLine(DisplayLine& l, int cols) {
  int left = cols;
  for (auto& s : l.spans) {
    if (static_cast<int>(s.text.size()) > left) s.text.resize(std::max(left, 0));
    left -= static_cast<int>(s.text.size());
  }
}

DisplayLine divider(const std::string& label, int cols, uint16_t fg) {
  DisplayLine l;
  l.divider = true;
  std::u32string t = U"\u2500\u2500 " + utf8ToU32(label) + U" ";
  while (static_cast<int>(t.size()) < cols) t += U'\u2500';
  l.spans.push_back({t, fg});
  clipLine(l, cols);
  return l;
}

std::u32string cutNick(const std::u32string& nick, int maxCols) {
  if (maxCols < 1) maxCols = 1;
  if (static_cast<int>(nick.size()) <= maxCols) return nick;
  return nick.substr(0, maxCols - 1) + U"\u2026";
}

}  // namespace

std::vector<DisplayLine> layoutRoom(const std::deque<CircMessage>& msgs, const LayoutOptions& o) {
  const Theme& th = o.theme ? *o.theme : defaultTheme();
  const int cols = std::max(o.cols, 1);
  const bool compact = o.size == TextSize::Compact;
  std::vector<DisplayLine> out;
  bool first = true;
  int64_t prevBucket = 0;

  for (const auto& m : msgs) {
    if (o.muted && o.muted->count(m.username)) continue;

    int64_t bucket = compact ? localDayNumber(m.timestamp) : localMinuteNumber(m.timestamp);
    if (first || bucket != prevBucket) {
      out.push_back(divider(compact ? formatDay(m.timestamp) : formatHHMM(m.timestamp), cols,
                            th.dim));
    }
    first = false;
    prevBucket = bucket;

    const bool self = !o.selfUsername.empty() && m.username == o.selfUsername;
    const uint16_t nickFg = nickColor(m.username, self);
    const bool tint =
        !m.deleted && !o.selfUsername.empty() && mentionsUser(m.content, o.selfUsername);
    const bool spoilerHidden =
        hasStyle(m, "spoiler") && !(o.revealed && o.revealed->count(m.id));
    std::u32string nick = utf8ToU32(m.username);

    // Header spans and where the body sits.
    DisplayLine head;
    int bodyStart, contIndent;
    const bool action = m.isAction && !m.deleted && !spoilerHidden;
    if (compact) {
      head.spans.push_back({utf8ToU32(formatHHMM(m.timestamp)) + U" ", th.time});
      nick = cutNick(nick, std::max(cols - kCompactTimeCols - 8, 1));
      if (action) {
        bodyStart = kCompactTimeCols;
        int under = kCompactTimeCols + 2 + static_cast<int>(nick.size()) + 1;
        contIndent = under <= kCompactIndentCap ? under : kCompactTimeCols;
      } else {
        head.spans.push_back({nick, nickFg});
        bodyStart = kCompactTimeCols + static_cast<int>(nick.size()) + 2;
        contIndent = bodyStart <= kCompactIndentCap ? bodyStart : kCompactTimeCols;
      }
    } else if (action) {
      bodyStart = 0;
      contIndent = kComfortBodyCol;
    } else {
      std::u32string n = cutNick(nick, kComfortNickCols);
      n.resize(kComfortNickCols, U' ');
      head.spans.push_back({n + U" ", nickFg});
      bodyStart = contIndent = kComfortBodyCol;
    }
    if (contIndent >= cols) contIndent = 0;

    // Body blocks: the main text, then one block per attachment.
    std::vector<Block> blocks;
    std::vector<std::u32string> artLines;
    bool isArt = false;
    if (m.deleted) {
      blocks.push_back({U"[deleted]", th.dim});
    } else if (spoilerHidden) {
      blocks.push_back({U"\u2592\u2592 spoiler \u00b7 \u25cf to reveal \u2592\u2592", th.sys});
    } else {
      std::string text;
      std::string art;
      if (hasStyle(m, "art") && base64Decode(m.content, art)) {
        isArt = true;
        std::u32string a = utf8ToU32(art);
        size_t start = 0;
        while (start <= a.size()) {
          size_t nl = a.find(U'\n', start);
          if (nl == std::u32string::npos) nl = a.size();
          std::u32string line = a.substr(start, nl - start);
          if (!line.empty() && line.back() == U'\r') line.pop_back();
          artLines.push_back(line);
          start = nl + 1;
        }
        while (!artLines.empty() && artLines.back().empty()) artLines.pop_back();
      } else if (m.isEightball) {
        text = "\xf0\x9f\x8e\xb1 " + m.content + " \xe2\x86\x92 " + m.eightballAnswer;
      } else if (m.isFortune) {
        text = "\xf0\x9f\xa5\xa0 " + m.fortuneText;
      } else if (!(m.content == m.imageUrl && !m.imageUrl.empty()) &&
                 !(m.content == m.gifUrl && !m.gifUrl.empty())) {
        text = m.content;
      }
      if (action) {
        blocks.push_back({U"* " + nick + (text.empty() ? U"" : U" " + utf8ToU32(text)), th.sys});
      } else if (!text.empty()) {
        blocks.push_back({utf8ToU32(text), th.fg});
      }
      if (!m.imageUrl.empty()) blocks.push_back({U"[img] " + utf8ToU32(m.imageUrl), th.dim});
      if (!m.gifUrl.empty()) blocks.push_back({U"[gif] " + utf8ToU32(m.gifUrl), th.dim});
      if (m.hasAudio) {
        std::string song = m.audio.artist.empty() || m.audio.title.empty()
                               ? m.audio.artist + m.audio.title
                               : m.audio.artist + " \xe2\x80\x93 " + m.audio.title;
        blocks.push_back({U"[song] " + utf8ToU32(song), th.dim});
      }
    }

    auto newLine = [&]() {
      DisplayLine l;
      l.messageId = m.id;
      l.mentionTint = tint;
      return l;
    };

    head.messageId = m.id;
    head.mentionTint = tint;
    std::vector<DisplayLine> lines;

    // A first line too narrow for text pushes the body to the next row.
    const bool roomOnHead = cols - bodyStart >= 4;
    if (isArt || blocks.empty() || !roomOnHead) {
      if (compact && !action) head.spans.push_back({U":", th.fg});
      if (!compact && !action) {
        // Drop the padding when there is nothing after the nick.
        auto& t = head.spans.back().text;
        while (!t.empty() && t.back() == U' ') t.pop_back();
      }
      lines.push_back(head);
    }

    for (const auto& line : artLines) {
      DisplayLine l = newLine();
      l.spans.push_back({line, th.fg});
      clipLine(l, cols);
      lines.push_back(l);
    }

    const int contWidth = std::max(cols - contIndent, 1);
    const std::u32string pad(contIndent, U' ');
    for (const auto& b : blocks) {
      bool onHead = lines.empty();
      int firstWidth = onHead ? std::max(cols - bodyStart, 1) : contWidth;
      auto wrapped = wrapText(b.text, firstWidth, contWidth);
      for (size_t i = 0; i < wrapped.size(); ++i) {
        DisplayLine l;
        if (onHead && i == 0) {
          l = head;
          if (compact && !action) l.spans.push_back({U": ", th.fg});
        } else {
          l = newLine();
          if (contIndent > 0) l.spans.push_back({pad, th.fg});
        }
        l.spans.push_back({wrapped[i], b.fg});
        lines.push_back(l);
      }
    }

    for (auto& l : lines) {
      if (lineWidth(l) > cols) clipLine(l, cols);
      out.push_back(std::move(l));
    }
  }
  return out;
}

}  // namespace rc
