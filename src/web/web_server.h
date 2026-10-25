#pragma once

namespace web {

// The config page and its JSON API on port 80 (esp_http_server), in both
// Wi-Fi modes. Every route checks the optional page password (Basic auth,
// user "admin").
void begin();
void stop();

}  // namespace web
