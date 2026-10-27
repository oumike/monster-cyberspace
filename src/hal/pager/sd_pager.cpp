#if defined(BOARD_PAGER)

#include <SD.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/pager/pins_pager.h"

namespace board {

bool hasSdSlot() { return true; }

// The shared SPI bus is up from begin(), with the other chip selects held
// high and the card's rail on. Slower clocks are tried for cards that won't
// start at the first (camillia-mt mounts this slot at 4 MHz, then 1 MHz).
fs::FS* sdMount() {
  for (uint32_t hz : {4000000u, 1000000u}) {
    if (SD.begin(pager::SD_CS, SPI, hz) && SD.cardType() != CARD_NONE) return &SD;
    SD.end();
  }
  return nullptr;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
