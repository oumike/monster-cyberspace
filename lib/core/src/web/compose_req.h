#pragma once

#include <string>
#include <string_view>

namespace rc {

// Web compose (boards with board::webCompose, the X4): text typed on the web
// config page, sent by the device.
//   open:   to what the device has open (a room, a conversation, a post)
//   mail:   a new C-Mail to `to`
//   thread: a new thread in guild `guild`, with an optional title
enum class ComposeKind { Open, Mail, Thread };

struct ComposeReq {
  ComposeKind kind = ComposeKind::Open;
  std::string text, to, guild, title;
};

// {"kind":"open|mail|thread","text":..,"to":..,"guild":..,"title":..}.
// "" when it parses, else "Bad request".
std::string parseCompose(std::string_view json, ComposeReq& r);
// Cleans r (trims; a leading '@' off `to`; chat text on one line) and
// returns "" when it can be sent, else the message to show. openTakesText:
// the device's open screen takes text.
std::string checkCompose(ComposeReq& r, bool openTakesText);

}  // namespace rc
