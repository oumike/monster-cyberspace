#pragma once

#include <cstdint>
#include <string>

#include "store/settings.h"

namespace rc::networker {

// Starts Wi-Fi (wifimgr::begin) and the network task on core 0 (12 KB
// stack). It serves commands from the queue, keeps the open room's streams
// and presence alive, and polls rooms and mentions every 60 s.
void start(const Settings&);

// Readable from any task (the web server).
int connState();  // -1 unknown, 0 offline, 1 reconnecting, 2 live
int authState();  // -1 unknown, 0 signed out, 1 signed in, 2 email unverified

// A login started by the web page: post a Login command after beginLogin(),
// then waitLogin() blocks until the worker finishes it or the timeout passes.
struct LoginResult {
  bool ok = false;
  int code = 0;  // the auth state afterwards
  std::string message;
};
uint32_t beginLogin();
bool waitLogin(uint32_t ticket, int timeoutMs, LoginResult& out);

}  // namespace rc::networker
