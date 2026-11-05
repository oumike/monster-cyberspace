#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "input/function_keys.h"
#include "input/key_event.h"

namespace rc {

// The coprocessor's keyboard state: 9 bytes, one bit per key.
constexpr int kTanmatsuKeyBytes = 9;
// Held Backspace, arrows and volume up repeat after kTanmatsuRepeatDelayMs,
// every kTanmatsuRepeatMs.
constexpr uint32_t kTanmatsuRepeatDelayMs = 450, kTanmatsuRepeatMs = 70;

// The Tanmatsu has F1-F6 only; Fn reaches the rest. F10's help shows this,
// and Ctrl tapped alone (emoji tray while typing, palette elsewhere).
constexpr FunctionKey kTanmatsuFunctionKeys[] = {
    {U"F1", U"home", Action::OpenHome},         {U"F2", U"messages", Action::OpenCirc},
    {U"F3", U"c-mail", Action::OpenCmail},      {U"F4", U"feed", Action::OpenFeed},
    {U"F5", U"guilds", Action::OpenMap},        {U"F6", U"profile", Action::OpenProfile},
    {U"Fn+F1", U"settings", Action::OpenSettings}, {U"Fn+F4", U"navigational help", Action::KeysHelp},
    {U"Fn+F5", U"keyboard light", Action::KbLightStep}, {U"Ctrl", U"emoji, palette", Action::Emoji},
};
constexpr size_t kTanmatsuFunctionKeyCount = std::size(kTanmatsuFunctionKeys);

// The Tanmatsu's keyboard, read whole from its coprocessor (bit order from
// badge-bsp's tanmatsu_coprocessor.h). A real matrix, so Shift, Fn, AltGr,
// Ctrl, left Alt and Meta are held like a desktop keyboard's. update() diffs
// a new state and queues its presses (Ctrl and the logo key tapped alone act
// on release); next() hands them out, then a held
// key's repeats. Keys as spec 2026-11-04-cyberspace-tanmatsu-design.md §4.
class TanmatsuKeys {
 public:
  void update(const uint8_t state[kTanmatsuKeyBytes], uint32_t ms);
  // The next queued key, else a held key's repeat; None when there is none.
  KeyEvent next(uint32_t ms);

 private:
  static constexpr int kQueue = 16;
  void push(const KeyEvent& e);

  uint8_t prev_[kTanmatsuKeyBytes] = {};
  KeyEvent queue_[kQueue];
  uint8_t head_ = 0, count_ = 0;
  bool metaUsed_ = false, ctrlUsed_ = false;
  int heldBit_ = -1;
  Action heldAction_ = Action::None;
  uint32_t nextRepeatMs_ = 0;
};

}  // namespace rc
