#pragma once

#include <cstddef>

#include "input/key_event.h"

namespace rc {

// Keyboards with a row of function keys (the T-Display P4's) jump straight to
// a place from anywhere; F10 shows this table, F11 steps the keyboard light.
struct FunctionKey {
  const char32_t* key;
  const char32_t* label;
  Action action;
};

constexpr FunctionKey kFunctionKeys[] = {
    {U"F1", U"messages", Action::OpenCirc},     {U"F2", U"rooms", Action::OpenRooms},
    {U"F3", U"c-mail", Action::OpenCmail},      {U"F4", U"feed", Action::OpenFeed},
    {U"F5", U"guilds", Action::OpenMap},        {U"F6", U"profile", Action::OpenProfile},
    {U"F7", U"settings", Action::OpenSettings}, {U"F10", U"navigational help", Action::KeysHelp},
    {U"F11", U"keyboard light", Action::KbLightStep},
};

// The action of function key n (1-based), or None.
constexpr Action functionKeyAction(int n) {
  switch (n) {
    case 1: return Action::OpenCirc;
    case 2: return Action::OpenRooms;
    case 3: return Action::OpenCmail;
    case 4: return Action::OpenFeed;
    case 5: return Action::OpenMap;
    case 6: return Action::OpenProfile;
    case 7: return Action::OpenSettings;
    case 10: return Action::KeysHelp;
    default: return Action::None;
  }
}

// The jumps a function key makes (not the help or the light).
constexpr bool isJump(Action a) {
  return a == Action::OpenCirc || a == Action::OpenRooms || a == Action::OpenCmail ||
         a == Action::OpenFeed || a == Action::OpenMap || a == Action::OpenProfile ||
         a == Action::OpenSettings || a == Action::OpenHome;
}

}  // namespace rc
