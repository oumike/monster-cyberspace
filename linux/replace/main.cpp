// SPDX-License-Identifier: GPL-3.0-or-later
// The Linux builds' entry point, in place of src/main.cpp (spec section 4.2):
// the same setup() steps, minus the NVS migration and OTA, then the HAL's
// main loop until a long Esc, a closed simulator window, SIGINT or SIGTERM.
#include <Arduino.h>
#include <sys/resource.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>

#include "app/app_state.h"
#include "app/time_service.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "core_version.h"
#include "feature_flags.h"
#include "hal/board.h"
#include "linux_hal.h"
#include "store/backup_store.h"
#include "store/locked_feed.h"
#if RC_FEAT_MASCOT
#include "store/mascot_store.h"
#endif
#include "store/settings.h"

using namespace rc;

namespace {

// The peak resident size, where the ESP builds log the internal heap.
void memMark(const char* step) {
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
#ifdef __APPLE__
  const long kb = u.ru_maxrss / 1024;  // bytes on macOS
#else
  const long kb = u.ru_maxrss;  // KB on Linux
#endif
  Serial.printf("[mem] %s: peak rss %ld KB\n", step, kb);
}

void onSignal(int) { requestQuit(); }

}  // namespace

int main() {
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  Serial.begin(115200);
  Serial.printf("[boot] Monster for Cyberspace %s on %s\n", RC_VERSION, board::name());
  board::begin();
  backupstore::begin();
  settings::load(app().cfg);
#if RC_FEAT_ENTRIES
  lockedFeed().with([](FeedState& s) { s.unreadFilter = app().cfg.unreadFilter; });
#endif
  board::setVolume(app().cfg.volume);
#if RC_FEAT_MASCOT
  mascotstore::load(app().mascot);
#endif
  timesvc::start(app().cfg);
  // The demo build (RC_FAKE_API) has no network.
  startChatSource(app().cfg);
  ui::start();
  memMark("setup done");

  linuxhal::runMainLoop(quitRequested);
  requestQuit();
  // Up to a second for the UI task to finish its frame and let go of LVGL.
  for (int i = 0; i < 100 && !ui::stopped(); ++i) usleep(10 * 1000);
  linuxhal::end();
  Serial.println("[boot] quit");
  std::fflush(stdout);
  // The workers may be blocked in a request: end without waiting for them or
  // running static destructors under them. Every save is already a whole file.
  _exit(0);
}
