#include "net/tls_alloc.h"

#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

#include <cstdlib>

namespace rc::tls {

namespace {

void* psramCalloc(size_t n, size_t size) {
  void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

}  // namespace

void usePsram() { mbedtls_platform_set_calloc_free(psramCalloc, free); }

}  // namespace rc::tls
