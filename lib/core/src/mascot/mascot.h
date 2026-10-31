#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rc {

// The splash and screensaver monster: its art and the phrases in its speech
// bubble. An empty vector means the default (Fluffy, or his words).
struct Mascot {
  std::vector<std::string> art, phrases;
};

constexpr size_t kMascotBlobSize = 4096;
constexpr size_t kMascotArtRows = 6;
constexpr size_t kMascotArtCols = 13;
constexpr size_t kMascotMaxPhrases = 10;
constexpr size_t kMascotPhraseMax = 80;

// The blob stored in the `mascot` flash partition (spec §3), padded with 0xFF
// to kMascotBlobSize. The caller validates first.
std::vector<uint8_t> encodeMascot(const Mascot& m);

// False on erased flash or any bad field, length or CRC; `out` is then
// unspecified.
bool decodeMascot(const uint8_t* data, size_t n, Mascot& out);

// "" when `m` fits the limits (spec §2), else a short reason.
std::string validateMascot(const Mascot& m);

// Trims trailing spaces from each art line, drops trailing blank art lines and
// drops empty phrases.
Mascot cleanMascot(Mascot m);

const std::vector<std::string>& defaultMascotArt();
const std::vector<std::string>& defaultMascotPhrases();

}  // namespace rc
