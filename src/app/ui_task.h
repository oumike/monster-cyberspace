#pragma once

#include <string>

#include "ui/grid.h"

namespace rc::ui {

// Starts the UI task on core 1 (16 KB stack, every 33 ms).
void start();
// Switches the text size, saves it and redraws everything.
void setTextSize(TextSize s);
// A one-row banner on the bottom row, above whatever the screen drew.
void banner(const std::string& text, bool error);
// Forces every row to be redrawn on the next frame.
void redrawAll();

}  // namespace rc::ui
