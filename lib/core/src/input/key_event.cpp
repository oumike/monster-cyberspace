#include "input/key_event.h"

namespace rc {

bool wakesScreen(const KeyEvent& e, bool anyInputWakes) {
  if (anyInputWakes) return e.action != Action::None;
  return e.action == Action::ScreenOff;
}

bool peeksScreen(const KeyEvent& e) {
  return e.action == Action::Select || e.action == Action::PowerTap;
}

}  // namespace rc
