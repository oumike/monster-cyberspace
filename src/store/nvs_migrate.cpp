#include "store/nvs_migrate.h"

#include <Arduino.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <memory>

namespace {

constexpr const char* kOld = "nvs_old";
constexpr const char* kNew = "nvs";

bool hasEntries() {
  nvs_iterator_t it = nullptr;
  esp_err_t e = nvs_entry_find(kOld, nullptr, NVS_TYPE_ANY, &it);
  nvs_release_iterator(it);
  return e == ESP_OK;
}

bool copyEntry(const nvs_entry_info_t& info) {
  nvs_handle_t src, dst;
  if (nvs_open_from_partition(kOld, info.namespace_name, NVS_READONLY, &src) != ESP_OK) return false;
  if (nvs_open_from_partition(kNew, info.namespace_name, NVS_READWRITE, &dst) != ESP_OK) {
    nvs_close(src);
    return false;
  }
  const char* k = info.key;
  esp_err_t e = ESP_FAIL;
  switch (info.type) {
#define RC_COPY(T, get, set) \
  {                          \
    T v;                     \
    e = get(src, k, &v);     \
    if (e == ESP_OK) e = set(dst, k, v); \
  }                          \
  break;
    case NVS_TYPE_U8: RC_COPY(uint8_t, nvs_get_u8, nvs_set_u8)
    case NVS_TYPE_I8: RC_COPY(int8_t, nvs_get_i8, nvs_set_i8)
    case NVS_TYPE_U16: RC_COPY(uint16_t, nvs_get_u16, nvs_set_u16)
    case NVS_TYPE_I16: RC_COPY(int16_t, nvs_get_i16, nvs_set_i16)
    case NVS_TYPE_U32: RC_COPY(uint32_t, nvs_get_u32, nvs_set_u32)
    case NVS_TYPE_I32: RC_COPY(int32_t, nvs_get_i32, nvs_set_i32)
    case NVS_TYPE_U64: RC_COPY(uint64_t, nvs_get_u64, nvs_set_u64)
    case NVS_TYPE_I64: RC_COPY(int64_t, nvs_get_i64, nvs_set_i64)
#undef RC_COPY
    case NVS_TYPE_STR: {
      size_t n = 0;
      e = nvs_get_str(src, k, nullptr, &n);
      if (e != ESP_OK) break;
      std::unique_ptr<char[]> buf(new char[n]);
      e = nvs_get_str(src, k, buf.get(), &n);
      if (e == ESP_OK) e = nvs_set_str(dst, k, buf.get());
      break;
    }
    case NVS_TYPE_BLOB: {
      size_t n = 0;
      e = nvs_get_blob(src, k, nullptr, &n);
      if (e != ESP_OK) break;
      std::unique_ptr<uint8_t[]> buf(new uint8_t[n ? n : 1]);
      e = nvs_get_blob(src, k, buf.get(), &n);
      if (e == ESP_OK) e = nvs_set_blob(dst, k, buf.get(), n);
      break;
    }
    default:
      break;
  }
  if (e == ESP_OK) e = nvs_commit(dst);
  nvs_close(dst);
  nvs_close(src);
  return e == ESP_OK;
}

}  // namespace

namespace nvsmigrate {

void run() {
  // Absent on boards still running the old table (an OTA never changes it).
  if (nvs_flash_init_partition(kOld) != ESP_OK) return;
  if (!hasEntries()) {
    nvs_flash_deinit_partition(kOld);
    return;
  }
  // Start nvs empty, so a copy cut short by power loss is simply redone.
  nvs_flash_deinit();
  if (nvs_flash_erase() != ESP_OK || nvs_flash_init() != ESP_OK) {
    Serial.println("[nvs] settings move failed: nvs unusable");
    nvs_flash_deinit_partition(kOld);
    return;
  }
  int moved = 0, failed = 0;
  nvs_iterator_t it = nullptr;
  esp_err_t e = nvs_entry_find(kOld, nullptr, NVS_TYPE_ANY, &it);
  while (e == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    copyEntry(info) ? ++moved : ++failed;
    e = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  Serial.printf("[nvs] moved %d settings to the new area (%d failed)\n", moved, failed);
  // One pass only: a retry would start nvs empty again and lose anything
  // saved since.
  nvs_flash_deinit_partition(kOld);
  nvs_flash_erase_partition(kOld);
}

}  // namespace nvsmigrate
