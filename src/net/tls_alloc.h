#pragma once

#include <cstddef>

namespace rc::tls {

// Points mbedTLS's allocator at PSRAM, falling back to internal RAM. Each TLS
// session holds two 16 KB record buffers; with the HTTPS and SSE sessions both
// open, internal RAM fragments until a new handshake can't allocate
// (mbedtls_ssl_setup -0x7F00). Without PSRAM it also sets aside two blocks
// for one session's record buffers. Call once, before any TLS connection.
void usePsram();
// The most mbedTLS has held at once.
size_t peakBytes();

}  // namespace rc::tls
