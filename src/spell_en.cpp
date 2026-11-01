// The compiled-in English word list (tools/gen_spell_dict.py). Firmware only:
// the host tests link lib/core's engine with a list of their own.
#include "feature_flags.h"

#if RC_FEAT_SPELL
#include "spell/spell.h"
#include "spell_dict_en.h"

namespace rc::spell {

const Dict &english() {
    static const Dict d = {
        kSpellEnData, kSpellEnSize, kSpellEnBlocks,
        (uint32_t)(sizeof(kSpellEnBlocks) / sizeof(kSpellEnBlocks[0])), kSpellEnWords,
    };
    return d;
}

}  // namespace rc::spell

#endif  // RC_FEAT_SPELL
