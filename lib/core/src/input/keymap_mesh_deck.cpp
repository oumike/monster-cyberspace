#include "input/keymap_mesh_deck.h"

namespace rc {
namespace {

constexpr uint8_t kKeyCount = 50;
constexpr uint8_t kShiftBit = 1, kSymBit = 2, kAltBit = 4;

// Codes below 0x08 are the keys that aren't characters.
constexpr char kNone = 0, kShift = 1, kAlt = 2, kSym = 3, kDel = 4, kEnter = 5, kTab = 6, kMic = 7;

constexpr char kMap[2][5][5] = {
    {
        {'1', '2', '3', '4', '5'},
        {'q', 'w', 'e', 'r', 't'},
        {'a', 's', 'd', 'f', 'g'},
        {kShift, 'z', 'x', 'c', 'v'},
        {kNone, kTab, kAlt, ',', ' '},
    },
    {
        {'6', '7', '8', '9', '0'},
        {'y', 'u', 'i', 'o', 'p'},
        {'h', 'j', 'k', 'l', kDel},
        {'b', 'n', 'm', kNone, kEnter},
        {kNone, '.', kMic, kSym, kAlt},
    },
};

char code(uint8_t pos) { return kMap[pos / 25][(pos % 25) / 5][pos % 5]; }

char shifted(char k) {
  if (k >= 'a' && k <= 'z') return char(k - 'a' + 'A');
  static const char kNum[] = ")!@#$%^&*(";
  if (k >= '0' && k <= '9') return kNum[k - '0'];
  if (k == ',') return '<';
  if (k == '.') return '>';
  return k;
}

char symbol(char k) {
  static const char kFrom[] = "qwertyuiopasdfghjklzxcvbnm,.";
  static const char kTo[] = "?/-_=+[]{}:;'\"`~\\|&<>*@#%^!?";
  for (int i = 0; kFrom[i]; ++i)
    if (kFrom[i] == k) return kTo[i];
  return k;
}

Action altChord(char k) {
  switch (k) {
    case 'w': return Action::Up;
    case 'a': return Action::Left;
    case 's': return Action::Down;
    case 'd': return Action::Right;
    case kDel: return Action::Back;
    case kEnter: return Action::Palette;
    case 'h': return Action::OpenHome;
    case 'c': return Action::OpenCirc;
    case 'm': return Action::OpenCmail;
    case 'f': return Action::OpenFeed;
    default: return Action::None;
  }
}

bool repeats(Action a) {
  return a == Action::Backspace || a == Action::Up || a == Action::Down || a == Action::Left ||
         a == Action::Right;
}

KeyEvent action(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

uint8_t modBit(char k) { return k == kShift ? kShiftBit : k == kSym ? kSymBit : k == kAlt ? kAltBit : 0; }

}  // namespace

KeyEvent MeshDeckKeys::key(uint8_t pos, bool pressed, uint32_t ms) {
  if (pos >= kKeyCount) return {};
  const char k = code(pos);
  const uint8_t bit = modBit(k);
  if (!pressed) {
    if (bit) held_ &= uint8_t(~bit);
    // Fn and Alt share a bit: either up ends a held chord's repeat.
    if (pos == repeatKey_ || (bit == kAltBit && repeatAction_ != Action::Backspace)) repeatKey_ = 0xFF;
    return {};
  }
  if (armed_ && ms - armedMs_ > kMeshDeckModifierMs) armed_ = 0;
  if (bit) {
    held_ |= bit;
    armed_ |= bit;
    armedMs_ = ms;
    return {};
  }
  if (k == kNone) return {};
  const uint8_t mods = held_ | armed_;
  armed_ = 0;
  KeyEvent e;
  if (mods & kAltBit) {
    e = action(altChord(k));
  } else if (k == kEnter) {
    e = action(mods & kSymBit ? Action::Symbols : Action::Select);
    e.modifier = e.action == Action::Symbols;
  } else if (k == kDel) {
    e = action(Action::Backspace);
  } else if (k == kTab) {
    e = action(Action::Palette);
  } else if (k == kMic) {
    e = action(Action::Emoji);
    e.modifier = true;
  } else {
    char c = k;
    if (mods & kSymBit) c = symbol(c);
    else if (mods & kShiftBit) c = shifted(c);
    e = action(Action::Char);
    e.ch = static_cast<char32_t>(c);
  }
  if (repeats(e.action)) {
    repeatKey_ = pos;
    repeatAction_ = e.action;
    repeatNextMs_ = ms + kMeshDeckRepeatDelayMs;
  }
  return e;
}

KeyEvent MeshDeckKeys::poll(uint32_t ms) {
  if (repeatKey_ == 0xFF || (int32_t)(ms - repeatNextMs_) < 0) return {};
  repeatNextMs_ = ms + kMeshDeckRepeatMs;
  return action(repeatAction_);
}

namespace {

Action padArrow(int b) {
  switch (1 << b) {
    case kPadDown: return Action::Down;
    case kPadLeft: return Action::Left;
    case kPadRight: return Action::Right;
    case kPadUp: return Action::Up;
    default: return Action::None;
  }
}

}  // namespace

int MeshDeckPad::update(uint8_t pressed, uint32_t ms, KeyEvent* out, int max) {
  int n = 0;
  auto emit = [&](Action a) {
    if (n < max) out[n++] = action(a);
  };
  for (int b = 0; b < 8; ++b) {
    const uint8_t m = uint8_t(1 << b);
    const Action arrow = padArrow(b);
    if (pressed & m) {
      releasing_ &= uint8_t(~m);
      if (!(down_ & m)) {
        down_ |= m;
        held_ &= uint8_t(~m);
        pressMs_[b] = ms;
        nextMs_[b] = ms + kMeshDeckRepeatDelayMs;
        if (arrow != Action::None) emit(arrow);
        else if (m == kPadR2 || m == kPadBoot) emit(Action::ScreenOff);
      } else if (arrow != Action::None) {
        if ((int32_t)(ms - nextMs_[b]) >= 0) {
          nextMs_[b] = ms + kMeshDeckRepeatMs;
          emit(arrow);
        }
      } else if ((m == kPadSelect || m == kPadL1) && !(held_ & m) && ms - pressMs_[b] >= kMeshDeckHoldMs) {
        held_ |= m;
        emit(m == kPadSelect ? Action::ScreenOff : Action::OpenHome);
      }
    } else if (down_ & m) {
      if (!(releasing_ & m)) {
        releasing_ |= m;
        continue;
      }
      releasing_ &= uint8_t(~m);
      down_ &= uint8_t(~m);
      if (!(held_ & m)) {
        if (m == kPadSelect) emit(Action::Select);
        else if (m == kPadL1) emit(Action::Back);
      }
      held_ &= uint8_t(~m);
    }
  }
  return n;
}

}  // namespace rc
