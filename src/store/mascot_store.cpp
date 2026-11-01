#include "store/mascot_store.h"

#include <Arduino.h>
#include <esp_flash.h>
#include <esp_partition.h>

#include <cstdlib>
#include <vector>

#include "app/queues.h"

namespace mascotstore {

namespace {

// Where partitions_16mb.csv puts it; used when the device still runs an older
// table (OTA keeps the table) that has no `mascot` entry.
constexpr uint32_t kFallbackAddr = 0xcb0000;

const esp_partition_t* partition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "mascot");
}

}  // namespace

void load(rc::Mascot& out) {
  out = rc::Mascot{};
  auto* buf = static_cast<uint8_t*>(malloc(rc::kMascotBlobSize));
  if (!buf) {
    Serial.println("[mascot] default");
    return;
  }
  const esp_partition_t* p = partition();
  esp_err_t err = p ? esp_partition_read(p, 0, buf, rc::kMascotBlobSize)
                    : esp_flash_read(esp_flash_default_chip, buf, kFallbackAddr, rc::kMascotBlobSize);
  rc::Mascot m;
  bool ok = err == ESP_OK && rc::decodeMascot(buf, rc::kMascotBlobSize, m);
  free(buf);
  if (ok) out = std::move(m);
  bool art = !out.art.empty(), phrases = !out.phrases.empty();
  Serial.println(art && phrases ? "[mascot] custom art and phrases"
                 : art          ? "[mascot] custom art"
                 : phrases      ? "[mascot] custom phrases"
                                : "[mascot] default");
}

bool save(const rc::Mascot& m) {
  std::vector<uint8_t> blob = rc::encodeMascot(m);
  if (blob.size() != rc::kMascotBlobSize) return false;
  const esp_partition_t* p = partition();
  esp_err_t err;
  if (p) {
    err = esp_partition_erase_range(p, 0, rc::kMascotBlobSize);
    if (err == ESP_OK) err = esp_partition_write(p, 0, blob.data(), blob.size());
  } else {
    err = esp_flash_erase_region(esp_flash_default_chip, kFallbackAddr, rc::kMascotBlobSize);
    if (err == ESP_OK) err = esp_flash_write(esp_flash_default_chip, blob.data(), kFallbackAddr, blob.size());
  }
  if (err != ESP_OK) {
    Serial.printf("[mascot] save failed: %s\n", esp_err_to_name(err));
    return false;
  }
  rc::postEvent(rc::Event{rc::EvtType::MascotChanged});
  return true;
}

}  // namespace mascotstore
