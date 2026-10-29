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

// The cyberspace.online themes. Colours from the site's own, as listed in
// z0mbieparade/cyberspace-atmospheric-modulator (src/shared/theme-colors.js,
// GPL-3.0); translucent ones are resolved over the background. The site has
// five per theme: text, background, dim text, borders (bars here) and code
// blocks (panels: the input line, hints and the member list).
constexpr Theme site(uint32_t bg, uint32_t fg, uint32_t dim, uint32_t bar, uint32_t panel, uint32_t mention,
                     uint32_t error) {
  return Theme{
      /*bg*/ rgb(bg),           /*fg*/ rgb(fg),        /*dim*/ rgb(dim),
      /*barBg*/ rgb(bar),       /*barFg*/ rgb(fg),     /*hintBg*/ rgb(panel),
      /*hintFg*/ rgb(dim),      /*time*/ rgb(dim),     /*sys*/ rgb(dim),
      /*mentionBg*/ rgb(mention), /*inputBg*/ rgb(panel), /*cursorBg*/ rgb(fg),
      /*cursorFg*/ rgb(bg),     /*selectBg*/ rgb(fg),  /*selectFg*/ rgb(bg),
      /*drawerBg*/ rgb(panel),  /*drawerEdge*/ rgb(dim), /*error*/ rgb(error),
      // One colour for nicks, as the site's themes have; yours is the dim one.
      /*colorNicks*/ false,     /*nick*/ rgb(fg),      /*selfNick*/ rgb(dim),
      /*outlined*/ false,
  };
}

// Dark: fg #efe5c0, bg #000000, dim #a89984, border #3a3a3a, code hsla(0,0%,100%,.07).
const Theme kDark = site(0x000000, 0xEFE5C0, 0xA89984, /*bar*/ 0x3A3A3A, /*panel*/ 0x121212,
                         /*mention*/ 0x232323, /*error*/ 0xFF5555);
// Light: fg #000000, bg #efe5c0, dim #3a3a3a, border #a89984, code rgba(0,0,0,.08).
const Theme kLight = site(0xEFE5C0, 0x000000, 0x3A3A3A, /*bar*/ 0xA89984, /*panel*/ 0xDCD3B1,
                         /*mention*/ 0xC4B79C, /*error*/ 0xC01818);
// LCD: fg #000000, bg #eaeae4, dim #3a3a3a, border #9aa49d, code #b4c2aa.
const Theme kLCD = site(0xEAEAE4, 0x000000, 0x3A3A3A, /*bar*/ 0x9AA49D, /*panel*/ 0xB4C2AA,
                         /*mention*/ 0xBAC0B9, /*error*/ 0xC01818);
// C64: fg hsla(0,0%,100%,.75), bg #2a2ab8, dim hsla(0,0%,100%,.4), border hsla(0,0%,100%,.3), code hsla(0,0%,100%,.08).
const Theme kC64 = site(0x2A2AB8, 0xCACAED, 0x7F7FD4, /*bar*/ 0x6A6ACD, /*panel*/ 0x3B3BBE,
                         /*mention*/ 0x5050C5, /*error*/ 0xFF5555);
// VT320: fg #ff9a10, bg #170800, dim #ff9100, border rgba(255,155,0,.27), code rgba(255,155,0,.05).
const Theme kVT320 = site(0x170800, 0xFF9A10, 0xFF9100, /*bar*/ 0x563000, /*panel*/ 0x230F00,
                         /*mention*/ 0x3D2000, /*error*/ 0xFF5555);
// Matrix: fg rgba(160,224,68,.9), bg #000000, dim rgba(160,224,68,.5), border rgba(160,224,68,.4), code rgba(0,255,65,.08).
const Theme kMatrix = site(0x000000, 0x90CA3D, 0x507022, /*bar*/ 0x405A1B, /*panel*/ 0x001405,
                         /*mention*/ 0x263610, /*error*/ 0xFF5555);
// Poetry: fg #222222, bg #fefaf8, dim #666666, border #cccccc, code #f0e0dd.
const Theme kPoetry = site(0xFEFAF8, 0x222222, 0x666666, /*bar*/ 0xCCCCCC, /*panel*/ 0xF0E0DD,
                         /*mention*/ 0xE0DEDE, /*error*/ 0xC01818);
// Brutalist: fg #c0d0e8, bg #080810, dim #99a9bf, border rgba(160,180,220,.18), code rgba(160,180,220,.06).
const Theme kBrutalist = site(0x080810, 0xC0D0E8, 0x99A9BF, /*bar*/ 0x232735, /*panel*/ 0x11121C,
                         /*mention*/ 0x181B26, /*error*/ 0xFF5555);
// GRiD: fg #fea813, bg #180f06, dim #d08c17, border rgba(245,169,28,.22), code rgba(245,169,28,.08).
const Theme kGrid = site(0x180F06, 0xFEA813, 0xD08C17, /*bar*/ 0x49310B, /*panel*/ 0x2A1B08,
                         /*mention*/ 0x352309, /*error*/ 0xFF5555);
// Crypt: fg #ee1100, bg #100202, dim rgba(255,0,0,.8), border rgba(255,0,0,.3), code rgba(255,0,0,.05).
const Theme kCrypt = site(0x100202, 0xEE1100, 0xCF0000, /*bar*/ 0x580101, /*panel*/ 0x1C0202,
                         /*mention*/ 0x3B0101, /*error*/ 0xFFFFFF);
// Bubblegum: fg #6b1a4a, bg #ffe4f1, dim #b86b93, border #f5b5d1, code #ffd0e5.
const Theme kBubblegum = site(0xFFE4F1, 0x6B1A4A, 0xB86B93, /*bar*/ 0xF5B5D1, /*panel*/ 0xFFD0E5,
                         /*mention*/ 0xF9C8DE, /*error*/ 0xC01818);
// Top8: fg #000000, bg #ffffff, dim #666666, border #6699cc, code #e6f2ff.
const Theme kTop8 = site(0xFFFFFF, 0x000000, 0x666666, /*bar*/ 0x6699CC, /*panel*/ 0xE6F2FF,
                         /*mention*/ 0xA3C2E0, /*error*/ 0xC01818);


// E-paper: black and white only.
const Theme kPaper = {
    /*bg*/ rgb(0xFFFFFF),        /*fg*/ rgb(0x000000),       /*dim*/ rgb(0x000000),
    /*barBg*/ rgb(0xFFFFFF),     /*barFg*/ rgb(0x000000),    /*hintBg*/ rgb(0xFFFFFF),
    /*hintFg*/ rgb(0x000000),    /*time*/ rgb(0x000000),     /*sys*/ rgb(0x000000),
    /*mentionBg*/ rgb(0xFFFFFF), /*inputBg*/ rgb(0xFFFFFF),  /*cursorBg*/ rgb(0x000000),
    /*cursorFg*/ rgb(0xFFFFFF),  /*selectBg*/ rgb(0x000000), /*selectFg*/ rgb(0xFFFFFF),
    /*drawerBg*/ rgb(0xFFFFFF),  /*drawerEdge*/ rgb(0x000000), /*error*/ rgb(0x000000),
    /*colorNicks*/ false,        /*nick*/ rgb(0x000000),     /*selfNick*/ rgb(0x000000),
    /*outlined*/ true,           /*boldNicks*/ true,
};

}  // namespace

const Theme& theme(ThemeId id) {
  switch (id) {
    case ThemeId::BlackWhite: return kBlackWhite;
    case ThemeId::WhiteBlack: return kWhiteBlack;
    case ThemeId::Dark: return kDark;
    case ThemeId::Light: return kLight;
    case ThemeId::Lcd: return kLCD;
    case ThemeId::C64: return kC64;
    case ThemeId::Vt320: return kVT320;
    case ThemeId::Matrix: return kMatrix;
    case ThemeId::Poetry: return kPoetry;
    case ThemeId::Brutalist: return kBrutalist;
    case ThemeId::Grid: return kGrid;
    case ThemeId::Crypt: return kCrypt;
    case ThemeId::Bubblegum: return kBubblegum;
    case ThemeId::Top8: return kTop8;
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
    case ThemeId::Dark: return "dark";
    case ThemeId::Light: return "light";
    case ThemeId::Lcd: return "lcd";
    case ThemeId::C64: return "c64";
    case ThemeId::Vt320: return "vt320";
    case ThemeId::Matrix: return "matrix";
    case ThemeId::Poetry: return "poetry";
    case ThemeId::Brutalist: return "brutalist";
    case ThemeId::Grid: return "grid";
    case ThemeId::Crypt: return "crypt";
    case ThemeId::Bubblegum: return "bubblegum";
    case ThemeId::Top8: return "top8";
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
    case ThemeId::Dark: return "Dark";
    case ThemeId::Light: return "Light";
    case ThemeId::Lcd: return "LCD";
    case ThemeId::C64: return "C64";
    case ThemeId::Vt320: return "VT320";
    case ThemeId::Matrix: return "Matrix";
    case ThemeId::Poetry: return "Poetry";
    case ThemeId::Brutalist: return "Brutalist";
    case ThemeId::Grid: return "GRiD";
    case ThemeId::Crypt: return "Crypt";
    case ThemeId::Bubblegum: return "Bubblegum";
    case ThemeId::Top8: return "Top8";
    default: return "Default";
  }
}

}  // namespace rc
