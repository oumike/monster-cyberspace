// SPDX-License-Identifier: GPL-3.0-or-later
// No mbedTLS here: libcurl's TLS uses the process heap.
#include "net/tls_alloc.h"

namespace rc::tls {

void usePsram() {}

size_t peakBytes() { return 0; }

}  // namespace rc::tls
