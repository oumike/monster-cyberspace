// SPDX-License-Identifier: GPL-3.0-or-later
// Web config on Linux, until the esp_http_server shim arrives (plan Task 7):
// the page is off.
#include "web/web_server.h"

namespace web {

void begin() {}
void stop() {}

}  // namespace web
