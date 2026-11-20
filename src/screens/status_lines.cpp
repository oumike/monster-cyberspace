// SPDX-License-Identifier: GPL-3.0-or-later
#include "screens/status_lines.h"

#include "app/app_state.h"
#include "feature_flags.h"
#if RC_WEB_OPT_IN
#include "web/web_server.h"
#endif

namespace rc {

// The status lines' labels, values and colours.
std::vector<StatusField> statusFields(const Theme& t, bool ap, const timesvc::Snapshot& ts) {
  const AppState& a = app();
  std::vector<StatusField> f;
  auto field = [&](const char* label, const std::string& value, uint16_t fg) {
    f.push_back(StatusField{label, value, fg});
  };
  field("account", a.user.empty() ? "not signed in" : "@" + a.user, t.fg);
  field("wi-fi", wifiUp() ? wifiSsid() + "  " + wifiIp() : "not connected", t.fg);
#if RC_WEB_OPT_IN
  field("config", !a.cfg.webConfig ? std::string("off")
                  : wifiUp()       ? "http://" + wifiIp() + ":" + std::to_string(web::kPort)
                                   : std::string("on, no network"),
        t.fg);
#else
  field("config", "http://monster-cyberspace.local", t.fg);
#endif
  int bat = batteryShown();
  field("battery", bat < 0 ? "unknown" : std::to_string(bat) + "%", t.fg);
  field("setup AP", ap ? "on" : "off", ap ? t.selectBg : t.fg);
  if (ap) {
    field("", setupApSsid(), t.selectBg);
    field("", setupApHasPassword() ? "password required" : "open network", t.dim);
    field("", "http://192.168.4.1", t.selectBg);
  }
  field("gps", ts.gps, t.fg);
  field("time", ts.time, t.fg);
  field("zone", ts.zone, t.fg);
  return f;
}

}  // namespace rc
