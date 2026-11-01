#if defined(BOARD_CARDPUTER_ADV)

#include <SD.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/cardputer_adv/pins_cardputer_adv.h"

namespace board {
namespace {

// The card has a bus of its own; the panel keeps the default one.
SPIClass sdSpi(HSPI);
bool busUp = false;

}  // namespace

bool hasSdSlot() { return true; }

// Slower clocks are tried for cards that won't start at the first.
fs::FS* sdMount() {
  if (!busUp) {
    sdSpi.begin(cardputer::SD_SCK, cardputer::SD_MISO, cardputer::SD_MOSI, cardputer::SD_CS);
    busUp = true;
  }
  for (uint32_t hz : {4000000u, 1000000u}) {
    if (SD.begin(cardputer::SD_CS, sdSpi, hz) && SD.cardType() != CARD_NONE) return &SD;
    SD.end();
  }
  return nullptr;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
