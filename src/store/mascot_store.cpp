#include "feature_flags.h"

#if RC_FEAT_MASCOT
#include "store/mascot_store.h"

#include <Arduino.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
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

// The raw fallback sector must not belong to some other partition.
bool fallbackFree() {
  bool ok = true;
  esp_partition_iterator_t it =
      esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  for (; it; it = esp_partition_next(it)) {
    const esp_partition_t* q = esp_partition_get(it);
    if (q->address < kFallbackAddr + rc::kMascotBlobSize && kFallbackAddr < q->address + q->size) {
      ok = false;
      break;
    }
  }
  esp_partition_iterator_release(it);
  if (!ok) Serial.println("[mascot] 0xcb0000 is in use");
  return ok;
}

}  // namespace

bool load(rc::Mascot& out) {
  const esp_partition_t* p = partition();
  if (!p && !fallbackFree()) return false;
  auto* buf = static_cast<uint8_t*>(malloc(rc::kMascotBlobSize));
  if (!buf) {
    Serial.println("[mascot] read failed: no memory");
    return false;
  }
  esp_err_t err = p ? esp_partition_read(p, 0, buf, rc::kMascotBlobSize)
                    : esp_flash_read(esp_flash_default_chip, buf, kFallbackAddr, rc::kMascotBlobSize);
  if (err != ESP_OK) {
    free(buf);
    Serial.printf("[mascot] read failed: %s\n", esp_err_to_name(err));
    return false;
  }
  rc::Mascot m;
  if (!rc::decodeMascot(buf, rc::kMascotBlobSize, m)) m = rc::Mascot{};
  free(buf);
  out = std::move(m);
  bool art = !out.art.empty(), phrases = !out.phrases.empty();
  Serial.println(art && phrases ? "[mascot] custom art and phrases"
                 : art          ? "[mascot] custom art"
                 : phrases      ? "[mascot] custom phrases"
                                : "[mascot] default");
  return true;
}

bool save(const rc::Mascot& m) {
  // Encoding allocates the 4 KB blob; on a tight heap (X4) that would throw.
  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < rc::kMascotBlobSize + 1024) {
    Serial.println("[mascot] save failed: no memory");
    return false;
  }
  const esp_partition_t* p = partition();
  if (!p && !fallbackFree()) return false;
  std::vector<uint8_t> blob = rc::encodeMascot(m);
  if (blob.size() != rc::kMascotBlobSize) return false;
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

#endif  // RC_FEAT_MASCOT
