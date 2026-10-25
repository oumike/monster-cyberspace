#include "net/sse_parser.h"

#include <string_view>

namespace rc {

void SseParser::reset() {
  line_.clear();
  event_.clear();
  data_.clear();
  hasData_ = false;
  afterCR_ = false;
  discarding_ = false;
  discardLineLen_ = 0;
  overflowed_ = false;
}

void SseParser::feed(const char* buf, size_t len, const Handler& onEvent) {
  for (size_t i = 0; i < len; ++i) {
    char c = buf[i];
    if (afterCR_) {
      afterCR_ = false;
      if (c == '\n') continue;  // second half of "\r\n"
    }
    if (c == '\r' || c == '\n') {
      afterCR_ = (c == '\r');
      endLine(onEvent);
      continue;
    }
    if (discarding_) {
      ++discardLineLen_;
      continue;
    }
    line_.push_back(c);
    if (line_.size() + data_.size() + event_.size() > maxEventBytes) {
      discarding_ = true;
      overflowed_ = true;
      discardLineLen_ = line_.size();
      line_.clear();
      line_.shrink_to_fit();
      event_.clear();
      data_.clear();
      data_.shrink_to_fit();
      hasData_ = false;
    }
  }
}

void SseParser::endLine(const Handler& onEvent) {
  if (discarding_) {
    if (discardLineLen_ == 0) discarding_ = false;  // blank line ends event
    discardLineLen_ = 0;
    return;
  }

  if (line_.empty()) {
    if (hasData_) {
      SseEvent ev{std::move(event_), std::move(data_)};
      overflowed_ = false;
      onEvent(ev);
    }
    event_.clear();
    data_.clear();
    hasData_ = false;
    return;
  }

  std::string_view l(line_);
  if (l[0] != ':') {
    std::string_view field = l, value;
    size_t colon = l.find(':');
    if (colon != std::string_view::npos) {
      field = l.substr(0, colon);
      value = l.substr(colon + 1);
      if (!value.empty() && value[0] == ' ') value.remove_prefix(1);
    }
    if (field == "event") {
      event_.assign(value);
    } else if (field == "data") {
      if (hasData_) data_.push_back('\n');
      data_.append(value);
      hasData_ = true;
    }
  }
  line_.clear();
}

}  // namespace rc
