// SPDX-License-Identifier: GPL-3.0-or-later
// One heap on Linux. heap_caps_free (linux/compat) is free().
#include "sys/mem.h"

#include <cstdlib>

namespace rc::mem {

void* big(size_t bytes) { return std::malloc(bytes); }

void* bigZeroed(size_t n, size_t size) { return std::calloc(n, size); }

}  // namespace rc::mem
