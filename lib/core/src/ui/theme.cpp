#include "ui/theme.h"

namespace rc {

namespace {

constexpr uint16_t rgb(uint32_t hex) {
  return static_cast<uint16_t>((((hex >> 16) & 0xFF) >> 3) << 11 | (((hex >> 8) & 0xFF) >> 2) << 5 |
                               ((hex & 0xFF) >> 3));
}

const Theme kDefault = {
    /*bg*/ rgb(0x000000),        /*fg*/ rgb(0xB8C0CC),       /*dim*/ rgb(0x555555),
    /*barBg*/ rgb(0x1B2A1B),     /*barFg*/ rgb(0x7CFC7C),    /*hintBg*/ rgb(0x141414),
    /*hintFg*/ rgb(0x666666),    /*time*/ rgb(0x5B9BFF),     /*sys*/ rgb(0x6C7A89),
    /*mentionBg*/ rgb(0x22223A), /*inputBg*/ rgb(0x0D0D0D),  /*cursorBg*/ rgb(0x7CFC7C),
    /*cursorFg*/ rgb(0x000000),  /*selectBg*/ rgb(0x7CFC7C), /*selectFg*/ rgb(0x000000),
    /*drawerBg*/ rgb(0x0A0F0A),  /*drawerEdge*/ rgb(0x7CFC7C), /*error*/ rgb(0xFF5555),
};

}  // namespace

const Theme& defaultTheme() { return kDefault; }

}  // namespace rc
