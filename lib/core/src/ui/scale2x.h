#pragma once

#include <cstdint>

// A logical frame drawn 2x onto a portrait native panel, in any of the four
// rotations of ui/rotation.h. "Native" here is the panel at half size
// (nw x nh); the caller doubles each native pixel.
namespace rc::scale2x {

struct Pt {
  int x, y;
};

// Frame size for rotation r: r odd (portrait) nw x nh, even nh x nw.
void frameSize(int nw, int nh, uint8_t r, int& w, int& h);
Pt frameToNative(int x, int y, int nw, int nh, uint8_t r);
Pt nativeToFrame(int nx, int ny, int nw, int nh, uint8_t r);  // inverse of frameToNative
// Native rows [ny0, ny1) that frame rows [y0, y1) touch.
void nativeRows(int y0, int y1, int nw, int nh, uint8_t r, int& ny0, int& ny1);
// Raw touch (clamped to the raw range) scaled to native.
Pt rawToNative(int rx, int ry, int rawW, int rawH, int nw, int nh);

}  // namespace rc::scale2x
