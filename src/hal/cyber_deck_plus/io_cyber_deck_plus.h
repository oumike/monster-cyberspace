#pragma once

#include <cstdint>

// What the Cyber Deck Plus adds to the Attaky Core (hal/attaky/attaky_core.h,
// shared with the Mesh Deck): the gamepad's two TCA9555 halves on the Core's
// I2C bus, Media I/O's NS4168 amplifier on I2S, and its microphone pins.
// Only the UI task calls the I2C functions, so the bus has no lock.
namespace cyber_deck_plus {

// Holds the microphone's clock and word-select low until something records
// (it is an I2S slave, clocked from us).
void parkMic();

// ── Gamepad (Control: Gamepad (Bottom) 1.0) ─────────────────────────────
// Both TCA9555 halves are all-input; pressed bits come back set, in the
// layout of rc::kCyberDeck* (input/keymap_cyber_deck.h).
// Brings up any half not yet found. False when neither answers.
bool gamepadProbe();
bool gamepadPresent();
// One read of both halves; false if neither answered this time.
bool readGamepad(uint8_t& left, uint8_t& right);

// ── Media I/O audio (NS4168 over I2S) ───────────────────────────────────
// Starts I2S and the task that plays tones. The amplifier's lines are always
// driven, never floating: a tone streams with silence around it, and after
// kIdleMs of silence the channel stops until the next tone. False: no audio.
bool audioBegin();
bool audioReady();
// Queues a tone; returns at once and the task plays it.
void audioTone(int freqHz, int ms);
// The next tone's loudness, as a percent (ui/volume.h).
void audioVolume(uint8_t pct);

}  // namespace cyber_deck_plus
