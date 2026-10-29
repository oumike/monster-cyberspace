#include "web/web_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_http_server.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/app_state.h"
#include "app/messages.h"
#include "app/net_worker.h"
#include "app/ota_service.h"
#include "app/queues.h"
#include "app/restart.h"
#include "app/time_service.h"
#include "chat/chat_source.h"
#include "config/backup_yaml.h"
#include "core_version.h"
#include "hal/board.h"
#include "net/wifi_list.h"
#include "net/wifi_mgr.h"
#include "store/backup_store.h"
#include "store/settings.h"
#include "tz/zones.h"
#include "ui/kb_light.h"
#include "ui/screen_timeout.h"
#include "web/page.h"
#include "web/web_util.h"

namespace web {

namespace {

using rc::Settings;

constexpr size_t kMaxBody = 1024;
constexpr int kLoginWaitMs = 20000;

httpd_handle_t server = nullptr;

void wipe(std::string& s) {
  std::fill(s.begin(), s.end(), '\0');
  s.clear();
}

bool printable(const std::string& s) {
  for (char c : s)
    if (c < 0x20 || c > 0x7E) return false;
  return true;
}

// Fresh from NVS on each request: the UI task owns the in-memory copy.
Settings current() {
  Settings s;
  settings::load(s);
  wipe(s.refreshToken);
  return s;
}

bool authorized(httpd_req_t* req) {
  Settings s = current();
  std::string header;
  size_t n = httpd_req_get_hdr_value_len(req, "Authorization");
  if (n > 0 && n < 512) {
    header.resize(n + 1);
    if (httpd_req_get_hdr_value_str(req, "Authorization", &header[0], n + 1) == ESP_OK)
      header.resize(n);
    else
      header.clear();
  }
  bool ok = rc::checkBasicAuth(header, s.pagePassword);
  wipe(s.pagePassword);
  wipe(s.apPassword);
  if (ok) return true;
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"monster-cyberspace\"");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "Password required");
  return false;
}

esp_err_t sendJson(httpd_req_t* req, const JsonDocument& doc, const char* status = "200 OK") {
  std::string out;
  serializeJson(doc, out);
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, out.data(), out.size());
}

esp_err_t fail(httpd_req_t* req, const char* status, const std::string& message) {
  JsonDocument d;
  d["ok"] = false;
  d["message"] = message;
  return sendJson(req, d, status);
}

esp_err_t okReply(httpd_req_t* req, const char* message = nullptr) {
  JsonDocument d;
  d["ok"] = true;
  if (message) d["message"] = message;
  return sendJson(req, d);
}

// Reads the body into `body` (zeroed by the caller when it holds secrets) and
// parses it in place, so parsed strings point into that buffer.
bool readJson(httpd_req_t* req, std::string& body, JsonDocument& doc) {
  if (req->content_len > kMaxBody) return false;
  body.assign(req->content_len, '\0');
  size_t got = 0;
  while (got < body.size()) {
    int r = httpd_req_recv(req, &body[got], body.size() - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0) return false;
    got += size_t(r);
  }
  return !deserializeJson(doc, &body[0], body.size());
}

// Reads the raw request body (the caller wipes it when it holds secrets).
bool readBody(httpd_req_t* req, std::string& body, size_t max) {
  if (req->content_len > max) return false;
  body.assign(req->content_len, '\0');
  size_t got = 0;
  while (got < body.size()) {
    int r = httpd_req_recv(req, &body[got], body.size() - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0) return false;
    got += size_t(r);
  }
  return true;
}

// Decodes the `name` query parameter. False unless it is a valid backup name,
// which the caller turns into "Bad file name"; runs before any store call.
bool queryName(httpd_req_t* req, std::string& name) {
  char query[256] = {0}, raw[160] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof query) != ESP_OK ||
      httpd_query_key_value(query, "name", raw, sizeof raw) != ESP_OK)
    return false;
  name = rc::urlDecode(raw);
  return rc::validBackupName(name);
}

// Sends `yaml` as a download named `name` (a validated name, so no escaping),
// then wipes it.
esp_err_t sendYamlAttachment(httpd_req_t* req, std::string& yaml, const std::string& name) {
  httpd_resp_set_type(req, "text/yaml; charset=utf-8");
  std::string disp = "attachment; filename=\"" + name + "\"";
  httpd_resp_set_hdr(req, "Content-Disposition", disp.c_str());
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  esp_err_t r = httpd_resp_send(req, yaml.data(), yaml.size());
  wipe(yaml);
  return r;
}

// ---- handlers ----

esp_err_t getPage(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t getStatus(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  Settings s = current();
  JsonDocument d;
  d["version"] = RC_VERSION;
  d["board"] = board::name();
  bool setup = wifimgr::mode() == wifimgr::Mode::Setup;
  d["mode"] = setup ? "setup" : "station";
  d["ip"] = wifimgr::ip();
  d["ssid"] = wifimgr::ssid();
  d["rssiBars"] = wifimgr::rssiBars();
  d["battery"] = board::batteryPercent();
  int auth = rc::networker::authState();
  d["signedIn"] = auth == 1 || auth == 2;
  d["verified"] = auth == 1;
  d["email"] = s.email;
  d["conn"] = rc::networker::connState();
  d["apSsid"] = wifimgr::apSsid();
  const rc::timesvc::Snapshot ts = rc::timesvc::snapshot();
  d["gps"] = ts.gps;
  d["time"] = ts.time;
  d["zone"] = ts.zone;
  JsonArray nets = d["saved"].to<JsonArray>();
  for (const auto& w : s.wifi) nets.add(w.ssid);
  d["pagePasswordSet"] = !s.pagePassword.empty();
  d["apPasswordSet"] = !s.apPassword.empty();
  wipe(s.pagePassword);
  wipe(s.apPassword);
  for (auto& w : s.wifi) wipe(w.password);
  return sendJson(req, d);
}

esp_err_t getScan(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  JsonDocument d;
  JsonArray a = d["networks"].to<JsonArray>();
  for (const auto& n : wifimgr::scan()) a.add(n);
  return sendJson(req, d);
}

esp_err_t postWifi(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string body;
  JsonDocument doc;
  if (!readJson(req, body, doc)) return fail(req, "400 Bad Request", "Bad request");
  std::string ssid = doc["ssid"] | "";
  std::string pass = doc["password"] | "";
  wipe(body);
  if (!rc::validWifiSsid(ssid)) return fail(req, "400 Bad Request", "Network name: 1-32 characters");
  if (!rc::validWifiPassword(pass)) {
    wipe(pass);
    return fail(req, "400 Bad Request", "Wi-Fi password: 8-63 characters, or empty for an open network");
  }
  Settings s = current();
  if (rc::saveWifiNet(s.wifi, ssid, pass) == rc::WifiSave::Full) {
    wipe(pass);
    for (auto& w : s.wifi) wipe(w.password);
    return fail(req, "400 Bad Request", "Five networks saved already: remove one first");
  }
  wipe(pass);
  settings::saveWifi(s.wifi);
  for (auto& w : s.wifi) wipe(w.password);
  if (wifimgr::mode() == wifimgr::Mode::Setup) {
    okReply(req, "Saved. Restarting to join it\u2026");
    rc::restartSoon(600);
    return ESP_OK;
  }
  return okReply(req, "Saved. It is used after a restart, or when the current network drops.");
}

esp_err_t deleteWifi(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  char query[160] = {0}, raw[100] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof query) != ESP_OK ||
      httpd_query_key_value(query, "ssid", raw, sizeof raw) != ESP_OK)
    return fail(req, "400 Bad Request", "Missing ssid");
  std::string ssid = rc::urlDecode(raw);
  Settings s = current();
  auto before = s.wifi.size();
  s.wifi.erase(std::remove_if(s.wifi.begin(), s.wifi.end(),
                              [&](const rc::WifiNet& w) { return w.ssid == ssid; }),
               s.wifi.end());
  if (s.wifi.size() == before) return fail(req, "404 Not Found", "Not saved");
  settings::saveWifi(s.wifi);
  for (auto& w : s.wifi) wipe(w.password);
  return okReply(req, s.wifi.empty() ? "Removed. With no networks saved, the next restart opens setup mode."
                                     : "Removed.");
}

esp_err_t postLogin(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string body;
  JsonDocument doc;
  if (!readJson(req, body, doc)) {
    wipe(body);
    return fail(req, "400 Bad Request", "Bad request");
  }
  rc::Command c{rc::CmdType::Login, doc["email"] | "", doc["password"] | ""};
  doc.clear();
  wipe(body);
  if (c.a.empty() || c.b.empty()) {
    wipe(c.b);
    return fail(req, "400 Bad Request", "Enter your email and password");
  }
  uint32_t ticket = rc::networker::beginLogin();
  rc::chatSource().command(c);
  wipe(c.b);
  rc::networker::LoginResult r;
  if (!rc::networker::waitLogin(ticket, kLoginWaitMs, r)) {
    r.ok = false;
    r.message = "Timed out: check the device";
  }
  JsonDocument d;
  d["ok"] = r.ok;
  d["code"] = r.code;
  d["message"] = r.message;
  return sendJson(req, d);
}

esp_err_t postLogout(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  rc::chatSource().command(rc::Command{rc::CmdType::Logout});
  return okReply(req, "Signed out");
}

esp_err_t postResend(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  rc::chatSource().command(rc::Command{rc::CmdType::ResendVerification});
  return okReply(req, "Requested: the device shows the result");
}

esp_err_t getPrefs(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  Settings s = current();
  JsonDocument d;
  d["textSize"] = rc::textSizeKey(s.textSize);
  d["buzzer"] = s.buzzer;
  d["screenOnSecs"] = s.screenOnSecs;
  d["screenSaver"] = s.screenSaver;
  d["spellCheck"] = s.spellCheck;
  if (!board::eink()) d["theme"] = rc::themeKey(s.theme);
  d["tz"] = s.tz;
  d["tzName"] = s.tzName;
  d["ntpServer"] = s.ntpServer;
  if (board::canRotate()) d["rotation"] = s.rotation;
  if (board::hasKbLight()) d["kbLight"] = rc::kbLightKey(s.kbLight);
  return sendJson(req, d);
}

esp_err_t getZones(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (size_t i = 0; i < rc::zoneCount(); ++i) {
    const rc::Zone& z = rc::zoneAt(i);
    JsonObject o = a.add<JsonObject>();
    o["iana"] = z.iana;
    o["label"] = z.label;
    o["posix"] = z.posix;
  }
  return sendJson(req, d);
}

esp_err_t postPrefs(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string body;
  JsonDocument doc;
  if (!readJson(req, body, doc)) return fail(req, "400 Bad Request", "Bad request");
  Settings s = current();
  rc::TextSize size = s.textSize;
  if (doc["textSize"].is<const char*>() && !rc::parseTextSize(doc["textSize"].as<const char*>(), size))
    return fail(req, "400 Bad Request", "Text size: compact, comfortable or large");
  bool buzzer = doc["buzzer"].is<bool>() ? doc["buzzer"].as<bool>() : s.buzzer;
  rc::ThemeId theme = s.theme;
  if (doc["theme"].is<const char*>() && !rc::parseThemeId(doc["theme"].as<const char*>(), theme))
    return fail(req, "400 Bad Request", "Theme: default, bw or wb");
  int rotation = -1;
  if (!doc["rotation"].isNull()) {
    if (!doc["rotation"].is<int>() || doc["rotation"].as<int>() < 0 || doc["rotation"].as<int>() > 3)
      return fail(req, "400 Bad Request", "Orientation: 0 to 3");
    rotation = doc["rotation"].as<int>();
  }
  uint32_t screenOnSecs = s.screenOnSecs;
  if (!doc["screenOnSecs"].isNull()) {
    if (!doc["screenOnSecs"].is<int>() || doc["screenOnSecs"].as<int>() < 0 ||
        !rc::validScreenTimeout(doc["screenOnSecs"].as<uint32_t>()))
      return fail(req, "400 Bad Request", "Screen timeout: 15, 30, 60, 120, 300, 600, 1800, 3600 or 0");
    screenOnSecs = doc["screenOnSecs"].as<uint32_t>();
  }
  const bool screenSaver = doc["screenSaver"].is<bool>() ? doc["screenSaver"].as<bool>() : s.screenSaver;
  const bool spellCheck = doc["spellCheck"].is<bool>() ? doc["spellCheck"].as<bool>() : s.spellCheck;
  rc::KbLight kbLight = s.kbLight;
  if (!doc["kbLight"].isNull() &&
      (!doc["kbLight"].is<const char*>() || !rc::parseKbLight(doc["kbLight"].as<const char*>(), kbLight)))
    return fail(req, "400 Bad Request", "Keyboard light: off, low, medium or high");
  std::string ntp = s.ntpServer;
  if (doc["ntpServer"].is<const char*>()) {
    ntp = doc["ntpServer"].as<const char*>();
    if (!rc::validNtpServer(ntp))
      return fail(req, "400 Bad Request", "NTP server: a host name, 1-63 characters");
  }
  // A listed zone name brings its own POSIX string. An empty name (custom)
  // takes "tz" as sent; "tz" alone is named from the list when it matches.
  std::string tz = s.tz, tzName = s.tzName;
  const bool named = doc["tzName"].is<const char*>();
  if (named) tzName = doc["tzName"].as<const char*>();
  if (named && !tzName.empty()) {
    const rc::Zone* z = rc::findZoneByIana(tzName);
    if (!z) return fail(req, "400 Bad Request", "Time zone: not in the list");
    tz = z->posix;
  } else if (doc["tz"].is<const char*>()) {
    tz = doc["tz"].as<const char*>();
    if (!rc::validPosixTz(tz))
      return fail(req, "400 Bad Request", "Time zone: a POSIX TZ string such as UTC0");
    if (!named) {
      const rc::Zone* z = rc::findZoneByPosix(tz);
      tzName = z ? z->iana : "";
    }
  }
  // The time service owns the TZ and SNTP; it reloads the saved values.
  if (ntp != s.ntpServer) settings::saveNtpServer(ntp);
  if (tz != s.tz || tzName != s.tzName) settings::saveTz(tz, tzName);
  if (ntp != s.ntpServer || tz != s.tz || tzName != s.tzName) rc::timesvc::settingsChanged();
  if (screenOnSecs != s.screenOnSecs || screenSaver != s.screenSaver) {
    rc::app().cfg.screenOnSecs = screenOnSecs;
    rc::app().cfg.screenSaver = screenSaver;
    settings::saveScreen(screenOnSecs, screenSaver);
  }
  if (spellCheck != s.spellCheck) {
    rc::app().cfg.spellCheck = spellCheck;
    settings::saveSpellCheck(spellCheck);
  }
  // The UI task picks the new level up on its next pass.
  if (board::hasKbLight() && kbLight != s.kbLight) {
    rc::app().cfg.kbLight = kbLight;
    settings::saveKbLight(kbLight);
  }
  // The UI task owns text size, theme and buzzer; it applies and saves them.
  if (!doc["textSize"].isNull() || !doc["buzzer"].isNull() || !doc["theme"].isNull())
    rc::postEvent(rc::Event{rc::EvtType::PrefsChanged, rc::themeKey(theme), static_cast<int>(size), buzzer});
  if (rotation >= 0 && board::canRotate() && rotation != s.rotation)
    rc::postEvent(rc::Event{rc::EvtType::RotationChanged, "", rotation, true});
  return okReply(req, "Saved");
}

esp_err_t postSecurity(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string body;
  JsonDocument doc;
  if (!readJson(req, body, doc)) {
    wipe(body);
    return fail(req, "400 Bad Request", "Bad request");
  }
  Settings s = current();
  std::string page = s.pagePassword, ap = s.apPassword;
  wipe(s.pagePassword);
  wipe(s.apPassword);
  if (doc["pagePassword"].is<const char*>()) page = doc["pagePassword"].as<const char*>();
  if (doc["apPassword"].is<const char*>()) ap = doc["apPassword"].as<const char*>();
  doc.clear();
  wipe(body);
  const char* err = nullptr;
  if (page.size() > 63 || !printable(page)) err = "Page password: up to 63 printable characters";
  else if (!rc::validApPassword(ap)) err = "Setup-AP password: 8-63 printable characters, or empty for an open AP";
  if (!err) settings::saveSecurity(page, ap);
  wipe(page);
  wipe(ap);
  if (err) return fail(req, "400 Bad Request", err);
  return okReply(req, "Saved. The setup-AP password applies the next time setup mode runs.");
}

esp_err_t getOta(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  const ota::Status st = ota::status();
  JsonDocument d;
  d["version"] = RC_VERSION;
  d["state"] = ota::stateName(st.state);
  d["latest"] = st.latest;
  d["error"] = st.error;
  d["percent"] = ota::percent(st);
  d["supported"] = st.layoutOk;
  d["auto"] = rc::app().cfg.autoUpdate;
  return sendJson(req, d);
}

esp_err_t postOtaCheck(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  if (!ota::startCheck()) return fail(req, "409 Conflict", "Can't check now: busy, or no Wi-Fi");
  return okReply(req, "Checking\u2026");
}

esp_err_t postOtaInstall(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  if (ota::status().state != ota::State::Available)
    return fail(req, "409 Conflict", "No update to install: check first");
  if (!ota::startInstall()) return fail(req, "409 Conflict", "Can't install now: busy, or no Wi-Fi");
  return okReply(req, "Installing: the device restarts when it is done");
}

esp_err_t postOtaAuto(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string body;
  JsonDocument doc;
  if (!readJson(req, body, doc) || !doc["auto"].is<bool>())
    return fail(req, "400 Bad Request", "Send {\"auto\":true} or {\"auto\":false}");
  const bool on = doc["auto"].as<bool>();
  settings::saveAutoUpdate(on);
  rc::app().cfg.autoUpdate = on;
  return okReply(req, on ? "Updates install on their own" : "Updates wait for you");
}

esp_err_t postRestart(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  okReply(req, "Restarting\u2026");
  rc::restartSoon(600);
  return ESP_OK;
}

esp_err_t postFactoryReset(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string body;
  JsonDocument doc;
  if (!readJson(req, body, doc) || !(doc["confirm"] | false))
    return fail(req, "400 Bad Request", "Send {\"confirm\":true} to erase everything");
  settings::factoryReset();
  okReply(req, "Erased. Restarting into setup mode\u2026");
  rc::restartSoon(600);
  return ESP_OK;
}

esp_err_t getBackup(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string yaml, name;
  backupstore::currentYaml(yaml, name);
  return sendYamlAttachment(req, yaml, name);
}

esp_err_t postRestore(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  if (req->content_len > rc::kMaxBackupBytes)
    return fail(req, "413 Payload Too Large", "Restore failed: Backup is over 16 KB");
  std::string body;
  if (!readBody(req, body, rc::kMaxBackupBytes)) {
    wipe(body);
    return fail(req, "400 Bad Request", "Bad request");
  }
  backupstore::Result r = backupstore::applyAndRestart(body);
  wipe(body);
  if (!r.ok) return fail(req, "400 Bad Request", r.message);
  return okReply(req, r.message.c_str());
}

esp_err_t getBackupSd(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  JsonDocument d;
  d["slot"] = board::hasSdSlot();
  std::vector<backupstore::FileInfo> files;
  backupstore::Result r = backupstore::listSd(files);
  d["card"] = r.ok;
  JsonArray a = d["files"].to<JsonArray>();
  for (const auto& f : files) {
    JsonObject o = a.add<JsonObject>();
    o["name"] = f.name;
    o["size"] = f.size;
  }
  return sendJson(req, d);
}

esp_err_t postBackupSd(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  backupstore::Result r = backupstore::saveToSd();
  if (!r.ok) return fail(req, "400 Bad Request", r.message);
  return okReply(req, r.message.c_str());
}

esp_err_t postBackupSdRestore(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string name;
  if (!queryName(req, name)) return fail(req, "400 Bad Request", "Bad file name");
  std::string yaml;
  backupstore::Result r = backupstore::readSd(name, yaml);
  if (!r.ok) {
    wipe(yaml);
    return fail(req, "400 Bad Request", r.message);
  }
  r = backupstore::applyAndRestart(yaml);
  wipe(yaml);
  if (!r.ok) return fail(req, "400 Bad Request", r.message);
  return okReply(req, r.message.c_str());
}

esp_err_t getBackupSdDownload(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string name;
  if (!queryName(req, name)) return fail(req, "400 Bad Request", "Bad file name");
  std::string yaml;
  backupstore::Result r = backupstore::readSd(name, yaml);
  if (!r.ok) {
    wipe(yaml);
    return fail(req, "400 Bad Request", r.message);
  }
  return sendYamlAttachment(req, yaml, name);
}

esp_err_t postBackupSdDelete(httpd_req_t* req) {
  if (!authorized(req)) return ESP_OK;
  std::string name;
  if (!queryName(req, name)) return fail(req, "400 Bad Request", "Bad file name");
  backupstore::Result r = backupstore::deleteSd(name);
  if (!r.ok) return fail(req, "400 Bad Request", r.message);
  return okReply(req, r.message.c_str());
}

}  // namespace

void begin() {
  if (server) return;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.max_uri_handlers = 28;
  cfg.stack_size = 8192;
  cfg.core_id = 0;
  cfg.lru_purge_enable = true;
  if (httpd_start(&server, &cfg) != ESP_OK) {
    Serial.println("[web] failed to start");
    server = nullptr;
    return;
  }
  struct Route {
    const char* uri;
    httpd_method_t method;
    esp_err_t (*fn)(httpd_req_t*);
  };
  static const Route routes[] = {
      {"/", HTTP_GET, getPage},
      {"/api/status", HTTP_GET, getStatus},
      {"/api/wifi/scan", HTTP_GET, getScan},
      {"/api/wifi", HTTP_POST, postWifi},
      {"/api/wifi", HTTP_DELETE, deleteWifi},
      {"/api/login", HTTP_POST, postLogin},
      {"/api/logout", HTTP_POST, postLogout},
      {"/api/resend", HTTP_POST, postResend},
      {"/api/prefs", HTTP_GET, getPrefs},
      {"/api/prefs", HTTP_POST, postPrefs},
      {"/api/zones", HTTP_GET, getZones},
      {"/api/security", HTTP_POST, postSecurity},
      {"/api/restart", HTTP_POST, postRestart},
      {"/api/factory-reset", HTTP_POST, postFactoryReset},
      {"/api/backup", HTTP_GET, getBackup},
      {"/api/restore", HTTP_POST, postRestore},
      {"/api/backup/sd", HTTP_GET, getBackupSd},
      {"/api/backup/sd", HTTP_POST, postBackupSd},
      {"/api/backup/sd/restore", HTTP_POST, postBackupSdRestore},
      {"/api/backup/sd/download", HTTP_GET, getBackupSdDownload},
      {"/api/backup/sd/delete", HTTP_POST, postBackupSdDelete},
      {"/api/ota", HTTP_GET, getOta},
      {"/api/ota/check", HTTP_POST, postOtaCheck},
      {"/api/ota/install", HTTP_POST, postOtaInstall},
      {"/api/ota/auto", HTTP_POST, postOtaAuto},
  };
  for (const Route& r : routes) {
    httpd_uri_t u = {};
    u.uri = r.uri;
    u.method = r.method;
    u.handler = r.fn;
    httpd_register_uri_handler(server, &u);
  }
  Serial.println("[web] listening on port 80");
}

void stop() {
  if (!server) return;
  httpd_stop(server);
  server = nullptr;
}

}  // namespace web
