#include "cs/token.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "util/base64.h"

namespace rc {

int64_t jwtExpSeconds(const std::string& jwt) {
  size_t a = jwt.find('.');
  if (a == std::string::npos) return -1;
  size_t b = jwt.find('.', a + 1);
  if (b == std::string::npos) return -1;
  std::string payload = jwt.substr(a + 1, b - a - 1);
  for (char& c : payload) {
    if (c == '-') c = '+';
    else if (c == '_') c = '/';
  }
  std::string json;
  if (payload.empty() || !base64Decode(payload, json)) return -1;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return -1;
  JsonVariantConst exp = doc["exp"];
  if (!exp.is<int64_t>() && !exp.is<double>()) return -1;
  return exp.as<int64_t>();
}

int64_t refreshDueMonoMs(int64_t receivedMonoMs, int64_t nowEpochSec, int64_t expSec) {
  if (nowEpochSec >= 1700000000 && expSec > 0) {
    int64_t leadSec = std::max<int64_t>(60, expSec - nowEpochSec - 300);
    return receivedMonoMs + leadSec * 1000;
  }
  return receivedMonoMs + 55LL * 60 * 1000;
}

}  // namespace rc
