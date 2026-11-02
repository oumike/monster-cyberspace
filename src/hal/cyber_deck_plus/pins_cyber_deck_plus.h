#pragma once

#include <cstdint>

// Attaky Cyber Deck Plus 1.0 pins. Source: Attaky datasheet
// "cyberdeck plus 1.0" (Core: ESP32 1.0, Expand: Media I/O 1.0,
// Control: Keyboard (Bottom) 1.0, Control: Gamepad (Bottom) 1.0,
// Power: Standard Cell 1.0). The Core's own pins (panel, I2C, front
// buttons, touch, gauge, keyboard module) are the Mesh Deck's, in
// hal/attaky/pins_attaky_core.h; these are what the deck adds.
//
// The build carries no GPS and no LoRa module, so IO17/IO18/IO21 are free for
// the Media I/O amplifier. On the Mesh Deck those same three pins are the GPS
// UART and the SX1262 DIO1, which is why a Mesh Deck build must never drive an
// amplifier there.
namespace cyber_deck_plus {

// Control: Gamepad (Bottom) 1.0, on the Core's I2C bus.
constexpr uint8_t PAD_LEFT_ADDR = 0x22, PAD_RIGHT_ADDR = 0x26;  // TCA9555 halves

// ── Expand: Media I/O 1.0 ───────────────────────────────────────────────
// NS4168 mono Class-D amplifier: BTB 31 / 32 / 39 -> IO17 / IO18 / IO21.
constexpr int SPK_BCLK = 17, SPK_SDATA = 18, SPK_LRCLK = 21;
constexpr uint32_t SPK_HZ = 16000;
// ICS-43434 microphone, on its own clocks: BTB 38/30/42/37 ->
// IO9 / IO16 / IO39 / IO46. Not driven yet (the app only outputs audio).
constexpr int MIC_WS = 9, MIC_SD = 16, MIC_SCK = 39, MIC_LR = 46;
// microSD slot: BTB 26/27/28/33 -> IO5 / IO6 / IO7 / IO8. Mounted over SDMMC
// in 1-bit mode (CLK, CMD, DAT0); DAT3 is left to the card's own pull-up.
constexpr int SD_CLK = 5, SD_CMD = 6, SD_DAT0 = 7, SD_DAT3 = 8;

}  // namespace cyber_deck_plus
