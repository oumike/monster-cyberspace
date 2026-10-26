#include "input/key_event.h"

namespace rc {

bool wakesScreen(const KeyEvent& e) { return e.action == Action::ScreenOff; }

bool peeksScreen(const KeyEvent& e) { return e.action == Action::Select; }

}  // namespace rc
