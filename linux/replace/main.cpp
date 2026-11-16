// SPDX-License-Identifier: GPL-3.0-or-later
// The Linux builds' entry point. For now (plan Task 1) it only starts LVGL on
// the HAL's panel and shows the app's name; Task 6 runs src/main.cpp's
// setup() steps here.
#include <lvgl.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "hal/board.h"
#include "linux_hal.h"
#include "lvui/display.h"
#include "lvui/lv_init.h"

namespace {

std::atomic<bool> quitting{false};
const auto started = std::chrono::steady_clock::now();

uint32_t ms() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started)
                      .count());
}

void uiThread() {
  lvui::setTickSource(ms);
  lvui::init();
  lvui::displayBegin({board::panelWidth(), board::panelHeight(), board::pushRows, 0});
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_t* label = lv_label_create(scr);
  lv_label_set_text_static(label, "Monster for Cyberspace\nCardputerZero");
  lv_obj_set_style_text_color(label, lv_color_hex(0x33ff66), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
  while (!quitting) {
    lv_timer_handler();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace

int main() {
  board::begin();
  std::thread ui(uiThread);
  linuxhal::runMainLoop([] { return quitting.load(); });
  quitting = true;
  ui.join();
  linuxhal::end();
  return 0;
}
