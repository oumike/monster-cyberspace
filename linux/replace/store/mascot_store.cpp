// SPDX-License-Identifier: GPL-3.0-or-later
// The custom mascot as the partition's blob, in mascot.bin in the config
// directory (spec section 4.2).
#include "feature_flags.h"

#if RC_FEAT_MASCOT
#include "store/mascot_store.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

#include "app/queues.h"

namespace mascotstore {

namespace {

const char* kFile = "mascot.bin";

std::string path() { return compat::configDir() + "/" + kFile; }

}  // namespace

bool load(rc::Mascot& out) {
  rc::Mascot m;
  FILE* f = std::fopen(path().c_str(), "rb");
  if (!f && errno != ENOENT) {
    Serial.printf("[mascot] read failed: %s\n", std::strerror(errno));
    return false;
  }
  if (f) {
    std::vector<uint8_t> buf(rc::kMascotBlobSize + 1);
    const size_t n = std::fread(buf.data(), 1, buf.size(), f);
    const bool failed = std::ferror(f);
    std::fclose(f);
    if (failed) {
      Serial.println("[mascot] read failed");
      return false;
    }
    if (n != rc::kMascotBlobSize || !rc::decodeMascot(buf.data(), n, m)) m = rc::Mascot{};
  }
  out = std::move(m);
  const bool art = !out.art.empty(), phrases = !out.phrases.empty();
  Serial.println(art && phrases ? "[mascot] custom art and phrases"
                 : art          ? "[mascot] custom art"
                 : phrases      ? "[mascot] custom phrases"
                                : "[mascot] default");
  return true;
}

bool save(const rc::Mascot& m) {
  const std::vector<uint8_t> blob = rc::encodeMascot(m);
  if (blob.size() != rc::kMascotBlobSize) return false;
  if (!compat::writeConfigFile(kFile, std::string(blob.begin(), blob.end()))) {
    Serial.printf("[mascot] save failed: %s\n", std::strerror(errno));
    return false;
  }
  rc::postEvent(rc::Event{rc::EvtType::MascotChanged});
  return true;
}

}  // namespace mascotstore

#endif  // RC_FEAT_MASCOT
