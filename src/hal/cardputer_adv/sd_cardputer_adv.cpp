#if defined(BOARD_CARDPUTER_ADV)

#include <SD.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/cardputer_adv/pins_cardputer_adv.h"

namespace board {
namespace {

// The card's bus is not the panel's (that one keeps the default SPI).
SPIClass sdSpi(HSPI);
bool busUp = false;

}  // namespace

bool hasSdSlot() { return true; }

// Slower clocks are tried for cards that won't start at the first. Both chip
// selects on the bus are held high first, as camillia-mt does.
fs::FS* sdMount() {
  if (!busUp) {
    pinMode(cardputer::CAP_LORA_CS, OUTPUT);
    digitalWrite(cardputer::CAP_LORA_CS, HIGH);
    pinMode(cardputer::SD_CS, OUTPUT);
    digitalWrite(cardputer::SD_CS, HIGH);
    sdSpi.begin(cardputer::SD_SCK, cardputer::SD_MISO, cardputer::SD_MOSI, -1);
    delay(8);
    busUp = true;
  }
  for (uint32_t hz : {4000000u, 1000000u, 400000u}) {
    if (SD.begin(cardputer::SD_CS, sdSpi, hz) && SD.cardType() != CARD_NONE) return &SD;
    SD.end();
  }
  return nullptr;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
