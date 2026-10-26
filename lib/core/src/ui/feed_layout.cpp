#include "ui/feed_layout.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "chat/message.h"
#include "feed/markdown_text.h"
#include "ui/nick_color.h"
#include "ui/wrap.h"
#include "util/utf8.h"

namespace rc {

namespace {

constexpr char32_t kSep[] = U" \u00b7 ";
constexpr char32_t kReadMore[] = U"\u2026 read the rest on the web";

std::u32string cut(std::u32string s, int cols) {
  if (static_cast<int>(s.size()) > cols) s.resize(std::max(cols, 0));
  return s;
}

bool hiddenByWarning(const Entry& e) { return !e.contentWarning.empty() || e.nsfw; }

std::u32string warningText(const Entry& e) {
  return U"CW: " + (e.contentWarning.empty() ? std::u32string(U"NSFW") : utf8ToU32(e.contentWarning));
}

void clip(DisplayLine& l, int cols) {
  int left = cols;
  for (auto& s : l.spans) {
    if (static_cast<int>(s.text.size()) > left) s.text.resize(std::max(left, 0));
    left -= static_cast<int>(s.text.size());
  }
}

void pushWrapped(std::vector<DisplayLine>& out, const std::u32string& text, uint16_t fg,
                 const std::string& id, int cols) {
  for (auto& w : wrapText(text, cols, cols)) {
    DisplayLine l;
    l.messageId = id;
    l.spans.push_back({std::move(w), fg});
    out.push_back(std::move(l));
  }
}

}  // namespace

std::string relativeAge(int64_t createdMs, int64_t nowMs) {
  int64_t s = std::max<int64_t>(nowMs - createdMs, 0) / 1000;
  if (s < 60) return "now";
  if (s < 3600) return std::to_string(s / 60) + "m";
  if (s < 86400) return std::to_string(s / 3600) + "h";
  if (s < 7 * 86400) return std::to_string(s / 86400) + "d";
  static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  std::time_t t = static_cast<std::time_t>(createdMs / 1000);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d %s", tm.tm_mday, kMonths[tm.tm_mon % 12]);
  return buf;
}

FeedRow layoutFeedRow(const Entry& e, bool bookmarked, int64_t nowMs, int cols, const Theme& t) {
  FeedRow r;
  r.line1 = utf8ToU32(e.author) + kSep + utf8ToU32(relativeAge(e.createdMs, nowMs)) + kSep +
            U"\u21a9" + utf8ToU32(std::to_string(e.replies));
  if (bookmarked) r.line1 += U" \u2605";
  if (e.deleted) {
    r.line2 = U"[deleted]";
  } else if (hiddenByWarning(e)) {
    r.line2 = warningText(e);
  } else if (!e.title.empty()) {
    r.line2 = utf8ToU32(e.title);
  } else {
    std::u32string text = markdownToText(e.body);
    size_t start = 0;
    while (start <= text.size()) {
      size_t nl = text.find(U'\n', start);
      if (nl == std::u32string::npos) nl = text.size();
      std::u32string line = text.substr(start, nl - start);
      if (line.find_first_not_of(U" \t\r") != std::u32string::npos) {
        r.line2 = line;
        break;
      }
      start = nl + 1;
    }
  }
  r.line1 = cut(r.line1, cols);
  r.line2 = cut(r.line2, cols);
  r.fg1 = nickColor(t, e.author, false);
  r.fg2 = t.dim;
  return r;
}

std::vector<DisplayLine> layoutEntry(const Entry& e, const std::deque<Reply>& replies,
                                     const EntryLayoutOptions& o) {
  const Theme& th = o.theme ? *o.theme : defaultTheme();
  const int cols = std::max(o.cols, 1);
  std::vector<DisplayLine> out;

  DisplayLine head;
  head.messageId = e.id;
  head.spans.push_back({utf8ToU32(e.author), nickColor(th, e.author, !o.self.empty() && e.author == o.self)});
  head.spans.push_back({kSep + utf8ToU32(relativeAge(e.createdMs, o.nowMs)), th.time});
  clip(head, cols);
  out.push_back(std::move(head));

  std::u32string marks;
  for (const auto& topic : e.topics) {
    if (!marks.empty()) marks += U' ';
    marks += U"#" + utf8ToU32(topic);
  }
  auto addMark = [&](const char32_t* m) {
    if (!marks.empty()) marks += kSep;
    marks += m;
  };
  if (e.isPublic) addMark(U"public");
  if (e.nsfw) addMark(U"NSFW");
  if (!marks.empty()) pushWrapped(out, marks, th.dim, e.id, cols);

  if (e.deleted) {
    pushWrapped(out, U"[deleted]", th.dim, e.id, cols);
  } else if (hiddenByWarning(e) && !o.revealed) {
    pushWrapped(out, warningText(e), th.sys, e.id, cols);
  } else {
    pushWrapped(out, markdownToText(e.body), th.fg, e.id, cols);
    for (const auto& a : e.attachments) pushWrapped(out, utf8ToU32(a), th.dim, e.id, cols);
    if (e.truncated) pushWrapped(out, kReadMore, th.dim, e.id, cols);
  }

  const int count = std::max(e.replies, static_cast<int>(replies.size()));
  DisplayLine div;
  div.divider = true;
  std::u32string label = U"\u2500\u2500 " + utf8ToU32(std::to_string(count)) +
                         (count == 1 ? U" reply " : U" replies ");
  while (static_cast<int>(label.size()) < cols) label += U'\u2500';
  div.spans.push_back({cut(label, cols), th.dim});
  out.push_back(std::move(div));

  LayoutOptions lo;
  lo.size = o.size;
  lo.cols = cols;
  lo.selfUsername = o.self;
  lo.theme = &th;
  for (const auto& r : replies) {
    if (!r.parentAuthor.empty()) {
      DisplayLine l;
      l.messageId = r.id;
      l.spans.push_back({U"\u21b3 @" + utf8ToU32(r.parentAuthor), th.dim});
      clip(l, cols);
      out.push_back(std::move(l));
    }
    CircMessage m;
    m.id = r.id;
    m.username = r.author;
    m.timestamp = r.createdMs;
    m.deleted = r.deleted;
    if (!r.deleted) {
      std::u32string text = markdownToText(r.body);
      for (const auto& a : r.attachments) text += U"\n" + utf8ToU32(a);
      if (r.truncated) text += std::u32string(U"\n") + kReadMore;
      m.content = u32ToUtf8(text);
    }
    for (auto& l : layoutRoom(std::deque<CircMessage>{m}, lo))
      if (!l.divider) out.push_back(std::move(l));
  }
  return out;
}

}  // namespace rc
