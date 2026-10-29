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
    /*colorNicks*/ true,         /*nick*/ rgb(0xB8C0CC),     /*selfNick*/ rgb(0xFFD166),
    /*outlined*/ false,
};

// White on black, greys only.
const Theme kBlackWhite = {
    /*bg*/ rgb(0x000000),        /*fg*/ rgb(0xFFFFFF),       /*dim*/ rgb(0x808080),
    /*barBg*/ rgb(0x303030),     /*barFg*/ rgb(0xFFFFFF),    /*hintBg*/ rgb(0x181818),
    /*hintFg*/ rgb(0x909090),    /*time*/ rgb(0xA0A0A0),     /*sys*/ rgb(0x909090),
    /*mentionBg*/ rgb(0x383838), /*inputBg*/ rgb(0x101010),  /*cursorBg*/ rgb(0xFFFFFF),
    /*cursorFg*/ rgb(0x000000),  /*selectBg*/ rgb(0xFFFFFF), /*selectFg*/ rgb(0x000000),
    /*drawerBg*/ rgb(0x101010),  /*drawerEdge*/ rgb(0xFFFFFF), /*error*/ rgb(0xFFFFFF),
    /*colorNicks*/ false,        /*nick*/ rgb(0xC8C8C8),     /*selfNick*/ rgb(0xFFFFFF),
    /*outlined*/ false,
};

// Black on white, greys only.
const Theme kWhiteBlack = {
    /*bg*/ rgb(0xFFFFFF),        /*fg*/ rgb(0x000000),       /*dim*/ rgb(0x707070),
    /*barBg*/ rgb(0xD0D0D0),     /*barFg*/ rgb(0x000000),    /*hintBg*/ rgb(0xE8E8E8),
    /*hintFg*/ rgb(0x606060),    /*time*/ rgb(0x505050),     /*sys*/ rgb(0x606060),
    /*mentionBg*/ rgb(0xD8D8D8), /*inputBg*/ rgb(0xF0F0F0),  /*cursorBg*/ rgb(0x000000),
    /*cursorFg*/ rgb(0xFFFFFF),  /*selectBg*/ rgb(0x000000), /*selectFg*/ rgb(0xFFFFFF),
    /*drawerBg*/ rgb(0xF0F0F0),  /*drawerEdge*/ rgb(0x000000), /*error*/ rgb(0x000000),
    /*colorNicks*/ false,        /*nick*/ rgb(0x383838),     /*selfNick*/ rgb(0x000000),
    /*outlined*/ false,
};

// E-paper: black and white only.
const Theme kPaper = {
    /*bg*/ rgb(0xFFFFFF),        /*fg*/ rgb(0x000000),       /*dim*/ rgb(0x000000),
    /*barBg*/ rgb(0xFFFFFF),     /*barFg*/ rgb(0x000000),    /*hintBg*/ rgb(0xFFFFFF),
    /*hintFg*/ rgb(0x000000),    /*time*/ rgb(0x000000),     /*sys*/ rgb(0x000000),
    /*mentionBg*/ rgb(0xFFFFFF), /*inputBg*/ rgb(0xFFFFFF),  /*cursorBg*/ rgb(0x000000),
    /*cursorFg*/ rgb(0xFFFFFF),  /*selectBg*/ rgb(0x000000), /*selectFg*/ rgb(0xFFFFFF),
    /*drawerBg*/ rgb(0xFFFFFF),  /*drawerEdge*/ rgb(0x000000), /*error*/ rgb(0x000000),
    /*colorNicks*/ false,        /*nick*/ rgb(0x000000),     /*selfNick*/ rgb(0x000000),
    /*outlined*/ true,
};

}  // namespace

const Theme& theme(ThemeId id) {
  switch (id) {
    case ThemeId::BlackWhite: return kBlackWhite;
    case ThemeId::WhiteBlack: return kWhiteBlack;
    default: return kDefault;
  }
}

const Theme& defaultTheme() { return kDefault; }

const Theme& paperTheme() { return kPaper; }

namespace {
bool gCursorShown = true;
}  // namespace

bool cursorShown() { return gCursorShown; }
void setCursorShown(bool on) { gCursorShown = on; }

ThemeId themeFromIndex(int i) {
  return i >= 0 && i < kThemeCount ? static_cast<ThemeId>(i) : ThemeId::Default;
}

ThemeId nextTheme(ThemeId id) { return themeFromIndex((static_cast<int>(id) + 1) % kThemeCount); }

const char* themeKey(ThemeId id) {
  switch (id) {
    case ThemeId::BlackWhite: return "bw";
    case ThemeId::WhiteBlack: return "wb";
    default: return "default";
  }
}

bool parseThemeId(std::string_view key, ThemeId& out) {
  for (int i = 0; i < kThemeCount; ++i) {
    if (key == themeKey(static_cast<ThemeId>(i))) {
      out = static_cast<ThemeId>(i);
      return true;
    }
  }
  return false;
}

const char* themeLabel(ThemeId id) {
  switch (id) {
    case ThemeId::BlackWhite: return "Black & white";
    case ThemeId::WhiteBlack: return "White & black";
    default: return "Default";
  }
}

}  // namespace rc
