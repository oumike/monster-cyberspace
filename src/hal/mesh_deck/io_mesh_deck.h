#pragma once

// What only the Mesh Deck has. The Core's I2C devices (buttons, touch, gauge,
// keyboard module, LEDs) are in hal/attaky/attaky_core.h.
namespace mesh_deck {

// Resets the LoRa module's SX1262, if one is fitted, and puts it to sleep
// with its chip select held high (left awake it whines). False: no radio.
bool radioSleep();

}  // namespace mesh_deck
