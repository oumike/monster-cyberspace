#pragma once

#include "input/key_event.h"

// The T-LoRa Pager's side wheel and the BOOT button. Turning the wheel moves
// Up or Down a detent at a time; pressing it selects (holding it 2 s turns
// the screen off). BOOT is Back, and held 0.6 s, Home.
namespace wheel {

void begin();
bool poll(rc::KeyEvent& key);

}  // namespace wheel
