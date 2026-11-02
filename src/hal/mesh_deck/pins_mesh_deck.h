#pragma once

#include <cstdint>

// Attaky Mesh Deck 1.0 pins. Sources: the Attaky hardware catalog,
// camillia-mt src/hal/hw_mesh_deck.h, platy-aprs src/hal/pins_mesh_deck.h.
// The Core's own pins (panel, I2C, buttons) are in hal/attaky/pins_attaky_core.h.
namespace mesh_deck {

// ATGM336H GPS, always powered.
constexpr int GPS_RX = 17, GPS_TX = 18;
constexpr uint32_t GPS_BAUD = 9600;

// The LoRa/GPS module's Ra-01SH (SX1262) on its own SPI pins (camillia-mt
// hw_mesh_deck.h). The firmware doesn't use the radio, and not every Mesh
// Deck has one: radioSleep() puts it to sleep at boot, if it answers.
constexpr int LORA_SCK = 5, LORA_MISO = 7, LORA_MOSI = 6, LORA_CS = 8;
constexpr int LORA_RST = 9, LORA_BUSY = 16, LORA_DIO1 = 21;

}  // namespace mesh_deck
