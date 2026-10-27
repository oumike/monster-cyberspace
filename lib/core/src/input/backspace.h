#pragma once

#include <cstdint>

namespace rc {

// On boards where Backspace doubles as Back (board::backspaceIsBack).
enum class BackspaceUse : uint8_t { Delete, Back, Drop };

// After the last deleting Backspace, a Backspace that would mean Back is
// dropped for this long, so holding it to clear a line doesn't then leave the
// screen.
constexpr uint32_t kBackspaceGuardMs = 400;

// consumes: the screen has text to delete. msSinceLastDelete: since the last
// Backspace that deleted (UINT32_MAX when none).
BackspaceUse resolveBackspace(bool consumes, uint32_t msSinceLastDelete);

}  // namespace rc
