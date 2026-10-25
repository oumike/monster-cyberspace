#pragma once

#include <ArduinoJson.h>

#include <string>

#include "chat/message.h"

namespace rc {

// Sets only the keys present in `o`. senderId/senderUsername (C-Mail) are
// read as userId/username. `style` may be a string or an array.
// When `deleted` becomes true, attachments, styles and dice/8-ball/fortune
// fields are cleared (the tombstone keeps author and timestamp).
void mergeMessageJson(CircMessage& m, JsonObjectConst o);
CircMessage messageFromJson(const std::string& id, JsonObjectConst o);

}  // namespace rc
