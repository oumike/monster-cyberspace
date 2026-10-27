#pragma once

#include <cstdint>
#include <string>

#include "ui/grid.h"

namespace rc::ui {

// Starts the UI task on core 1 (16 KB stack, every 33 ms).
void start();
// Switches the text size, saves it and redraws everything.
void setTextSize(TextSize s);
// The menu item "Text size: <next>" and what picking it does.
std::u32string textSizeItem();
void cycleTextSize();
// Turns the panel (rotation r, ui/rotation.h) at once, saves it and reflows
// the grid. Only on boards that can rotate.
void setRotation(uint8_t r);
// A one-row banner on the bottom row, above whatever the screen drew.
void banner(const std::string& text, bool error);
// Forces every row to be redrawn on the next frame.
void redrawAll();

}  // namespace rc::ui
