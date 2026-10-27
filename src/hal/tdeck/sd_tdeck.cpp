#if defined(BOARD_TDECK)

#include <SD.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/tdeck/pins_tdeck.h"

namespace board {

bool hasSdSlot() { return true; }

// The shared SPI bus is up from begin(), with LoRa CS held high throughout. A
// card reporting CARD_NONE counts as no card.
fs::FS* sdMount() {
  if (!SD.begin(tdeck::SD_CS, SPI, tdeck::SD_SPI_HZ)) {
    SD.end();
    return nullptr;
  }
  if (SD.cardType() == CARD_NONE) {
    SD.end();
    return nullptr;
  }
  return &SD;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
