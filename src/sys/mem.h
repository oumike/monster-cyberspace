#pragma once

#include <cstddef>

namespace rc::mem {

// Large buffers: PSRAM when the board has it, otherwise internal RAM (the
// X4's ESP32-C3 has none). Free with heap_caps_free.
void* big(size_t bytes);
void* bigZeroed(size_t n, size_t size);

}  // namespace rc::mem
