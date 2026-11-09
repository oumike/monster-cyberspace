#include "lvui/lv_init.h"

#include <lvgl.h>

#include <cstdio>
#include <cstdlib>

namespace lvui {

namespace {
void reportAndAbort(const char* file, int line) {
  std::fprintf(stderr, "[lvgl] assert failed at %s:%d\n", file, line);
  std::abort();
}
void (*assertHandler)(const char*, int) = reportAndAbort;

uint32_t (*tickSource)() = nullptr;
uint32_t tick() { return tickSource ? tickSource() : 0; }
bool started = false;
}  // namespace

void setTickSource(uint32_t (*ms)()) {
  tickSource = ms;
  if (started) lv_tick_set_cb(tick);
}

void setAssertHandler(void (*handler)(const char* file, int line)) {
  assertHandler = handler ? handler : reportAndAbort;
}

void init() {
  if (started) return;
  lv_init();
  lv_tick_set_cb(tick);
  started = true;
}

}  // namespace lvui

// Here, in the object init() pulls into the link, so LVGL's own archive finds it.
extern "C" void lvui_assert_failed(const char* file, int line) { lvui::assertHandler(file, line); }

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM
#include <esp_heap_caps.h>
#include <sdkconfig.h>

#ifndef RC_LV_HEAP
#define RC_LV_HEAP 49152
#endif

namespace {
#ifdef BOARD_HAS_PSRAM
constexpr uint32_t kCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#else
// No PSRAM: LVGL shares the internal heap with TLS, so it gets a budget.
constexpr uint32_t kCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
size_t used = 0;
size_t maxUsed = 0;
#if defined(CONFIG_HEAP_POISONING_LIGHT) || defined(CONFIG_HEAP_POISONING_COMPREHENSIVE)
// Heap poisoning wraps each block in an 8 B head (canary, size) and a 4 B
// tail canary, which heap_caps_get_allocated_size leaves out.
constexpr size_t kPoison = 12;
#else
constexpr size_t kPoison = 0;
#endif
// What a block of n bytes takes from the heap: n and the poisoning, rounded
// to 4 B (12 B at least), and TLSF's 4 B header. `used`, the budget and
// lv_mem_monitor count this, so they match heap_caps_get_free_size.
size_t cost(size_t n) {
  const size_t b = (n + kPoison + 3) & ~size_t(3);
  return (b < 12 ? 12 : b) + 4;
}
#endif
}  // namespace

extern "C" {

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *, size_t) { return nullptr; }
void lv_mem_remove_pool(lv_mem_pool_t) {}

#ifdef BOARD_HAS_PSRAM
void *lv_malloc_core(size_t size) { return heap_caps_malloc(size, kCaps); }
void *lv_realloc_core(void *p, size_t size) { return heap_caps_realloc(p, size, kCaps); }
void lv_free_core(void *p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t *mon) {
  mon->total_size = heap_caps_get_total_size(kCaps);
  mon->free_size = heap_caps_get_free_size(kCaps);
  mon->free_biggest_size = heap_caps_get_largest_free_block(kCaps);
  mon->used_pct = mon->total_size
                      ? (uint8_t)(100 - mon->free_size * 100 / mon->total_size)
                      : 0;
}
#else
void *lv_malloc_core(size_t size) {
  if (used + cost(size) > RC_LV_HEAP) return nullptr;
  void *p = heap_caps_malloc(size, kCaps);
  if (p) {
    used += cost(heap_caps_get_allocated_size(p));
    if (used > maxUsed) maxUsed = used;
  }
  return p;
}

void *lv_realloc_core(void *p, size_t size) {
  const size_t old = p ? cost(heap_caps_get_allocated_size(p)) : 0;
  const size_t want = size ? cost(size) : 0;
  if (want > old && used - old + want > RC_LV_HEAP) return nullptr;
  void *q = heap_caps_realloc(p, size, kCaps);
  if (q) {
    used = used - old + cost(heap_caps_get_allocated_size(q));
    if (used > maxUsed) maxUsed = used;
  } else if (size == 0) {
    used -= old;
  }
  return q;
}

void lv_free_core(void *p) {
  if (!p) return;
  used -= cost(heap_caps_get_allocated_size(p));
  heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon) {
  mon->total_size = RC_LV_HEAP;
  mon->free_size = used < RC_LV_HEAP ? RC_LV_HEAP - used : 0;
  mon->free_biggest_size = mon->free_size;
  mon->max_used = maxUsed;
  mon->used_pct = (uint8_t)(used * 100 / RC_LV_HEAP);
}
#endif

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

}  // extern "C"
#endif
