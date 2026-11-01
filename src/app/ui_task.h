#pragma once

#include <cstdint>
#include <string>

#include "ui/grid.h"
#include "ui/notify_light.h"
#include "web/compose_req.h"

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
// A one-row banner on the top row, over the status bar.
void banner(const std::string& text, bool error);
// The key being handled came from the touch screen (a tap, drag or the
// on-screen keyboard), not a button or keyboard.
bool fromTouch();
// The next word for the monster's speech bubble (ui/splash_layout.h).
std::string monsterWord();
// Docks the on-screen keyboard after this key, on touch boards without a
// keyboard, if the screen then takes text.
void showKeyboard();
// Boards that can't type (board::canType): shows "<what> from web config"
// and returns true, so the caller skips its text field.
bool typingElsewhere(const char* what);
// Web compose (board::webCompose): what the device has open, read from any
// task, and a request handed to the UI task, which sends it as the device
// would. webCompose returns "" when taken, else why not.
struct ComposeView {
  std::string label;  // "#room", "C-Mail with @ann", "replies to the open post"; "" when none
  bool takesText = false;
};
ComposeView composeView();
std::string webCompose(ComposeReq r);
// Forces every row to be redrawn on the next frame.
void redrawAll();
// One double blink of the new-message light in c (board::hasNotifyLight).
// UI task only; the web server posts EvtType::NotifyPreview instead.
void notifyLightPreview(LedColor c);

}  // namespace rc::ui
