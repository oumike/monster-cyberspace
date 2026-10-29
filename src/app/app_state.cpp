#include "app/app_state.h"

#include <sys/time.h>

#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "screens/router.h"
#include "ui/timefmt.h"
#include "ui/widgets/status_bar.h"

namespace rc {

AppState& app() {
  static AppState a;
  return a;
}

const Theme& currentTheme() { return board::eink() ? paperTheme() : theme(app().cfg.theme); }

int64_t nowEpochMs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

std::string clockText() {
  int64_t ms = nowEpochMs();
  return ms < 1700000000000LL ? std::string("--:--") : formatHHMM(ms);
}

bool wifiUp() { return wifimgr::connected(); }
std::string wifiSsid() { return wifimgr::ssid(); }
std::string wifiIp() { return wifimgr::ip(); }
int wifiBars() { return wifimgr::rssiBars(); }
bool setupApUp() { return wifimgr::mode() == wifimgr::Mode::Setup; }
std::string setupApSsid() { return wifimgr::apSsid(); }
bool setupApHasPassword() { return wifimgr::apHasPassword(); }

void drawStatus(Grid& g, const Theme& t, std::u32string_view title, int count, bool menu) {
  char32_t marker = app().conn == 2 ? U'\u25CF' : app().conn == 1 ? U'\u25CC' : U'\u2715';
  StatusTouch touch{board::hasTouch(), router::current() != router::Id::Home, menu};
  renderStatusBar(g, t, title, count, marker, wifiBars(), board::batteryPercent(), clockText(),
                  touch);
}

}  // namespace rc
