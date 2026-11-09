#include "ui/room_layout.h"

#include <algorithm>
#include <cctype>

#include "chat/mention.h"
#include "ui/nick_color.h"
#include "ui/timefmt.h"
#include "ui/text_width.h"
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
  for (const auto& s : l.spans) n += textWidth(s.text);
  return n;
}

void clipLine(DisplayLine& l, int cols) {
  int left = cols;
  for (auto& s : l.spans) {
    s.text.resize(fitChars(s.text, std::max(left, 0)));
    left -= textWidth(s.text);
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

// ChatStyle::Bubbles: the meta line (nick and time; yours only the time, on
// the right), then the blocks wrapped to about three quarters of the width in a
// bubble with a column of padding each side, set a column in from its edge.
void bubbleLines(const CircMessage& m, const LayoutOptions& o, const Theme& th, int cols, bool self,
                 bool action, uint16_t nickFg, bool tint, const std::vector<Block>& blocks,
                 const std::vector<std::u32string>& artLines,
                 const std::function<void(DisplayLine&&)>& out) {
  auto newLine = [&]() {
    DisplayLine l;
    l.messageId = m.id;
    l.mentionTint = tint;
    return l;
  };
  const std::u32string time = utf8ToU32(formatHHMM(m.timestamp));
  DisplayLine meta = newLine();
  meta.bubbleMeta = true;
  // The time always shows: the nick, then the reply's parent, give way to it.
  const std::u32string parent = utf8ToU32(o.metaExtra);
  const int parentW = textWidth(parent);
  // "↳ @parent" cut to fit `room` columns, or empty when even a short one won't.
  auto replyTo = [&](int room) -> std::u32string {
    if (parent.empty() || room < 5) return U"";
    return U"\u21b3 @" + (parentW + 3 <= room ? parent : cutNick(parent, room - 3));
  };
  const int timeW = static_cast<int>(time.size());
  if (self) {
    const std::u32string extra = replyTo(cols - timeW - 1);
    const int extraW = extra.empty() ? 0 : textWidth(extra) + 1;
    const int pad = cols - timeW - extraW;
    if (pad > 0) meta.spans.push_back({std::u32string(pad, U' '), th.fg});
    if (!extra.empty()) meta.spans.push_back({extra + U" ", th.dim});
    meta.spans.push_back({time, th.time});
  } else if (action) {
    meta.spans.push_back({time, th.time});
  } else {
    const int avail = cols - timeW - 1;  // before " HH:MM"
    const std::u32string nick = utf8ToU32(m.username);
    const int nickW = textWidth(nick);
    // The nick keeps up to 6 columns before the parent is cut.
    const int replyFull = parent.empty() ? 0 : parentW + 4;
    const int nickMax = std::max(std::min(nickW, 6), avail - replyFull);
    const std::u32string shown = cutNick(nick, std::min(nickMax, avail));
    meta.spans.push_back({shown, nickFg, th.boldNicks});
    const std::u32string extra = replyTo(avail - textWidth(shown) - 1);
    if (!extra.empty()) meta.spans.push_back({U" " + extra, th.dim});
    meta.spans.push_back({U" " + time, th.time});
  }
  clipLine(meta, cols);
  out(std::move(meta));

  // The bubble sits a column in from its edge, under the meta line's start
  // (or end, for yours); none on a log too narrow to spare it.
  const int indent = cols >= 8 ? 1 : 0;
  const int inner = std::max(1, std::min(std::max(8, cols * 3 / 4) - 2, cols - 2 - indent));
  std::vector<Span> rows;
  for (const auto& a : artLines) rows.push_back({a.substr(0, fitChars(a, inner)), th.fg});
  for (const auto& b : blocks)
    for (auto& w : wrapText(b.text, inner, inner)) rows.push_back({w, b.fg});
  if (rows.empty()) return;
  int longest = 0;
  for (const auto& r : rows) longest = std::max(longest, textWidth(r.text));
  const int w = std::min(longest + 2, cols - indent);
  const int col = self ? cols - w - indent : indent;
  for (size_t i = 0; i < rows.size(); ++i) {
    DisplayLine l = newLine();
    l.bubble = self ? DisplayLine::Bubble::Self : DisplayLine::Bubble::Other;
    l.bubbleCol = static_cast<uint8_t>(col);
    l.bubbleCols = static_cast<uint8_t>(w);
    l.bubbleTop = i == 0;
    l.bubbleBottom = i + 1 == rows.size();
    if (col > 0) l.spans.push_back({std::u32string(col, U' '), th.fg});
    l.spans.push_back({U" " + rows[i].text, rows[i].fg});
    clipLine(l, cols);
    out(std::move(l));
  }
}

}  // namespace

std::vector<DisplayLine> layoutRoom(const std::deque<CircMessage>& msgs, const LayoutOptions& o) {
  std::vector<DisplayLine> out;
  layoutRoomEach(msgs, o, 0, msgs.size(), [&](DisplayLine&& l) { out.push_back(std::move(l)); });
  return out;
}

void layoutRoomEach(const std::deque<CircMessage>& msgs, const LayoutOptions& o, size_t from,
                    size_t to, const std::function<void(DisplayLine&&)>& out) {
  const Theme& th = o.theme ? *o.theme : defaultTheme();
  const int cols = std::max(o.cols, 1);
  const bool compact = o.size == TextSize::Compact;
  auto isMuted = [&](const CircMessage& m) {
    if (!o.muted) return false;
    std::string name = m.username;
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o.muted->count(name) > 0;
  };
  auto bucketOf = [&](const CircMessage& m) {
    return compact ? localDayNumber(m.timestamp) : localMinuteNumber(m.timestamp);
  };
  to = std::min(to, msgs.size());
  bool first = true;
  int64_t prevBucket = 0;
  // The divider depends on the message shown before the range.
  for (size_t i = std::min(from, msgs.size()); i-- > 0;) {
    if (isMuted(msgs[i])) continue;
    first = false;
    prevBucket = bucketOf(msgs[i]);
    break;
  }

  for (size_t mi = from; mi < to; ++mi) {
    const CircMessage& m = msgs[mi];
    if (isMuted(m)) continue;

    int64_t bucket = bucketOf(m);
    if (first || bucket != prevBucket) {
      out(divider(compact ? formatDay(m.timestamp) : formatHHMM(m.timestamp), cols, th.dim));
    }
    first = false;
    prevBucket = bucket;

    const bool self = !o.selfUsername.empty() && sameUsername(m.username, o.selfUsername);
    const uint16_t nickFg = nickColor(th, m.username, self);
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
        contIndent = o.indentToNick                ? kCompactTimeCols + 2
                     : under <= kCompactIndentCap ? under
                                                  : kCompactTimeCols;
      } else {
        head.spans.push_back({nick, nickFg, th.boldNicks});
        bodyStart = kCompactTimeCols + static_cast<int>(nick.size()) + 2;
        contIndent = !o.indentToNick && bodyStart <= kCompactIndentCap ? bodyStart : kCompactTimeCols;
      }
    } else if (action) {
      bodyStart = 0;
      contIndent = kComfortBodyCol;
    } else {
      std::u32string n = cutNick(nick, kComfortNickCols);
      n.resize(kComfortNickCols, U' ');
      head.spans.push_back({n + U" ", nickFg, th.boldNicks});
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

    if (o.style == ChatStyle::Bubbles) {
      bubbleLines(m, o, th, cols, self, action, nickFg, tint, blocks, artLines, out);
      continue;
    }

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
      out(std::move(l));
    }
  }
}

}  // namespace rc
