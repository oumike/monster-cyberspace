#include "net/tls_alloc.h"

#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

#include <Arduino.h>

#include <atomic>
#include <cstdlib>
#include <cstring>

namespace rc::tls {

namespace {

// What mbedTLS holds, and its peak, for the [tls] log.
size_t held = 0, peak = 0;

#if RC_LOW_MEM
// Without PSRAM the heap fragments once a room is open, and a session's two
// 16 KB record buffers (16717 bytes each) no longer find a contiguous block.
// Two blocks set aside at start serve only those.
constexpr size_t kSlotMin = 16 * 1024;
constexpr size_t kSlotSize = 16 * 1024 + 512;
struct Slot {
  void* mem = nullptr;
  std::atomic<bool> used{false};
};
Slot slots[2];

void* slotTake(size_t bytes) {
  if (bytes < kSlotMin || bytes > kSlotSize) return nullptr;
  for (Slot& s : slots) {
    bool expected = false;
    if (s.mem && s.used.compare_exchange_strong(expected, true)) {
      memset(s.mem, 0, bytes);
      return s.mem;
    }
  }
  return nullptr;
}

bool slotGive(void* p) {
  for (Slot& s : slots) {
    if (p && s.mem == p) {
      s.used = false;
      return true;
    }
  }
  return false;
}
#endif

void* psramCalloc(size_t n, size_t size) {
#if RC_LOW_MEM
  if (void* p = slotTake(n * size)) {
    held += kSlotSize;
    if (held > peak) peak = held;
    return p;
  }
#endif
  void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (p) {
    held += heap_caps_get_allocated_size(p);
    if (held > peak) peak = held;
  } else {
    Serial.printf("[tls] alloc %u failed: tls holds %u, free %u blk %u\n", unsigned(n * size), unsigned(held),
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                  unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  }
  return p;
}

void tlsFree(void* p) {
#if RC_LOW_MEM
  if (slotGive(p)) {
    held -= kSlotSize;
    return;
  }
#endif
  if (p) held -= heap_caps_get_allocated_size(p);
  free(p);
}

}  // namespace

void usePsram() {
#if RC_LOW_MEM
  for (Slot& s : slots)
    if (!s.mem) s.mem = heap_caps_malloc(kSlotSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
  mbedtls_platform_set_calloc_free(psramCalloc, tlsFree);
}

size_t peakBytes() { return peak; }

}  // namespace rc::tls
