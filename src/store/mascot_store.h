#pragma once

#include "mascot/mascot.h"

// The custom mascot (art and phrases) in its own 4 KB `mascot` flash
// partition (spec §3). OTA never writes it and factory reset (NVS) keeps it.
namespace mascotstore {

// Reads and decodes the blob. An erased or invalid blob is a good read that
// sets `out` empty (the defaults). Returns false, leaving `out` unchanged, when
// the read itself fails (no memory, flash error, fallback sector in use).
// Logs one `[mascot]` line, never the content.
bool load(rc::Mascot& out);

// Erases and writes the sector, then posts EvtType::MascotChanged so the UI
// task reloads. Callable from any task; never touches app state. The caller
// validates first.
bool save(const rc::Mascot& m);

}  // namespace mascotstore
