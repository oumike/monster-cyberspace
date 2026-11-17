#pragma once

#include "feature_flags.h"

namespace web {

// The config page and its JSON API on port 80 (8080 with RC_WEB_OPT_IN) (esp_http_server), in both
// Wi-Fi modes. Every route checks the optional page password (Basic auth,
// user "admin").
void begin();
void stop();

#if RC_WEB_OPT_IN
// Linux: port 8080, and begin() serves only while Settings' "Web config
// page" is on and a page password is set. apply() starts or stops the server
// to match the saved settings (safe from a request handler: the server stops
// once that request is answered).
constexpr int kPort = 8080;
void apply();
bool running();
#endif

}  // namespace web
