#pragma once

#include "mascot/mascot.h"

// The custom mascot (art and phrases) in its own 4 KB `mascot` flash
// partition (spec §3). OTA never writes it and factory reset (NVS) keeps it.
namespace mascotstore {

// Reads and decodes the blob; erased or bad flash leaves `out` empty (the
// defaults). Logs one `[mascot]` line, never the content.
void load(rc::Mascot& out);

// Erases and writes the sector, then posts EvtType::MascotChanged so the UI
// task reloads. Callable from any task; never touches app state. The caller
// validates first.
bool save(const rc::Mascot& m);

}  // namespace mascotstore
