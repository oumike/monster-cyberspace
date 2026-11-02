#if defined(BOARD_MESH_DECK)

#include "hal/mesh_deck/io_mesh_deck.h"

#include <Arduino.h>

#include "hal/mesh_deck/pins_mesh_deck.h"

// The Core's I2C devices are shared with the Cyber Deck Plus
// (hal/attaky/attaky_core.cpp). This holds what only the Mesh Deck has: the
// LoRa/GPS module's SX1262.
namespace mesh_deck {

bool radioSleep() {
  // Chip select high first, through its pull-up (the core writes only to
  // output pins): a falling edge would wake it.
  pinMode(LORA_CS, INPUT_PULLUP);
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_SCK, OUTPUT);
  digitalWrite(LORA_SCK, LOW);
  pinMode(LORA_MOSI, OUTPUT);
  digitalWrite(LORA_MOSI, LOW);
  pinMode(LORA_MISO, INPUT_PULLUP);
  pinMode(LORA_DIO1, INPUT_PULLDOWN);
  // Pulled up, BUSY stays high with no module; a radio drops it once reset.
  pinMode(LORA_BUSY, INPUT_PULLUP);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW);
  delay(2);
  digitalWrite(LORA_RST, HIGH);
  const uint32_t t0 = millis();
  while (digitalRead(LORA_BUSY) == HIGH) {
    if (millis() - t0 > 50) return false;
    delay(1);
  }
  // SetSleep (0x84), cold start: SPI mode 0, MSB first, bit-banged.
  const uint8_t cmd[] = {0x84, 0x00};
  digitalWrite(LORA_CS, LOW);
  delayMicroseconds(1);
  for (uint8_t b : cmd) {
    for (int i = 7; i >= 0; --i) {
      digitalWrite(LORA_MOSI, (b >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(LORA_SCK, HIGH);
      delayMicroseconds(1);
      digitalWrite(LORA_SCK, LOW);
    }
  }
  delayMicroseconds(1);
  digitalWrite(LORA_CS, HIGH);
  digitalWrite(LORA_MOSI, LOW);
  return true;
}

}  // namespace mesh_deck

#endif
