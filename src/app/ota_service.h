#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Over-the-air updates straight from this repo's GitHub releases, over HTTPS
// with the same certificate bundle the Cyberspace client uses:
//   https://github.com/oumike/monster-cyberspace/releases/latest  -> tag (redirect)
//   .../releases/download/<tag>/monster-cyberspace-<slug>-<tag>-ota.bin
//   ... -ota.bin.sig   detached ECDSA P-256 signature (scripts/release.sh)
// The image is hashed as it streams and the signature checked against the key
// baked into the firmware (ota_signing_pubkey.h) before the new slot is made
// bootable. Settings live in NVS, which an update never touches.
//
// A background task checks 20 s after joining Wi-Fi and then daily, and
// installs on its own only when Settings::autoUpdate is on. Every other check
// or install comes from a key press, a tap or the web page.
namespace ota {

enum class State : uint8_t { Idle, Checking, UpToDate, Available, Installing, Done, Failed };

struct Status {
  State state = State::Idle;
  std::string latest;  // newest release tag, once a check succeeded
  std::string error;   // set in Failed
  size_t written = 0, total = 0;  // install progress (total 0 = unknown)
  bool layoutOk = true;           // false on a flash layout OTA must not touch
};

// Starts the scheduler task. Call once from setup().
void start();
Status status();
const char* stateName(State s);
// False when busy, off Wi-Fi, already installed, or on an unsupported layout.
bool startCheck();
// Installs the latest release found by the last check; restarts when done.
bool startInstall();
// 0-100, or -1 when the size is unknown.
int percent(const Status& s);

}  // namespace ota
