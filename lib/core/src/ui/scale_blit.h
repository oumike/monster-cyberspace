#pragma once

#include <cstdint>

namespace rc {

// A rectangle on the panel, in native (portrait) pixels.
struct BlitRect {
  int x, y, w, h;
};

// For panels the renderer draws at a fraction of their resolution, turned in
// software: copies frame rows [y0, y1) of an fw x fh frame, laid out for
// rotation r (ui/rotation.h), into `out` as the native-portrait panel
// rectangle they cover, each frame pixel a scale x scale block. `out` is
// row-major, the returned rect's w wide, and holds fw * (y1 - y0) * scale^2
// pixels. The mapping is the inverse of input/touch_transform's touchToFrame,
// so a touch lands on what is drawn under it.
BlitRect scaleBlitRows(const uint16_t* frame, int fw, int fh, int y0, int y1, uint8_t r, int scale,
                       uint16_t* out);

}  // namespace rc
