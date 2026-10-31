#pragma once

// The meaningful bytes of the mascot blob for art {"/\_/\", "(o o)"} and
// phrases {"boo", "meow"}, made with Python's zlib (plan Task 1, Step 1). The
// rest of the 4096 bytes is 0xFF.

namespace fx {

constexpr const char* kMascotGoldenHex =
    "46 4c 55 46 01 02 02 00 15 00 05 2f 5c 5f 2f 5c 05 28 6f 20 6f 29 03 62 6f 6f 04 6d 65 6f 77 "
    "c4 c5 34 01";

}  // namespace fx
