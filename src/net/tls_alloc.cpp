#include "net/tls_alloc.h"

#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

#include <Arduino.h>

#include <cstdlib>

namespace rc::tls {

namespace {

// What mbedTLS holds, and its peak, for the [tls] log.
size_t held = 0, peak = 0;

void* psramCalloc(size_t n, size_t size) {
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
  if (p) held -= heap_caps_get_allocated_size(p);
  free(p);
}

}  // namespace

void usePsram() { mbedtls_platform_set_calloc_free(psramCalloc, tlsFree); }

size_t peakBytes() { return peak; }

}  // namespace rc::tls
