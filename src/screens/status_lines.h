// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The device's status lines (account, Wi-Fi, config, battery, setup AP, GPS,
// time, zone): in the middle of Home where homeShowsStatus(), else at the top
// of Settings. Never both.

#include <string>
#include <vector>

#include "app/time_service.h"
#include "hal/board.h"
#include "ui/theme.h"
#include "ui/wrap.h"
#include "util/utf8.h"

namespace rc {

struct StatusField {
  const char* label;
  std::string value;
  uint16_t fg;
};

// The status lines' labels, values and colours.
std::vector<StatusField> statusFields(const Theme& t, bool ap, const timesvc::Snapshot& ts);

// The status lines from row 1, put(row, label, line, fg) for each line only
// above row `end`, `shift` rows already off the top (label: on a value's
// first line, else null). Returns the rows they take.
// On a narrow grid a value too long for its row continues on the next rows.
template <typename Put>
int layoutStatus(const std::vector<StatusField>& fields, bool narrow, int valueW, int shift, int end, Put put) {
  int r = 1;
  for (const StatusField& f : fields) {
    const std::vector<std::u32string> lines =
        narrow ? wrapText(utf8ToU32(f.value), valueW, valueW)
               : std::vector<std::u32string>{utf8ToU32(f.value)};
    for (size_t i = 0; i < lines.size(); ++i, ++r) {
      const int row = r - shift;
      if (row < 1 || row >= end) continue;
      put(row, i == 0 ? f.label : nullptr, lines[i], f.fg);
    }
  }
  return r - 1;
}

// Whether Home shows the status lines in this orientation
// (board::homeStatus()); Settings then leaves them out.
inline bool homeShowsStatus() { return board::homeStatus(); }

}  // namespace rc
