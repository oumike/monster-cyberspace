#include "input/keymap_tanmatsu.h"

namespace rc {
namespace {

constexpr int kKeyCount = kTanmatsuKeyBytes * 8;

enum : int {
  kEsc = 0, kF1 = 1, kF2 = 2, kF3 = 3, kTab = 8, kFn = 12, kShiftL = 16, kCtrl = 20,
  kMeta = 21, kAltL = 22, kF4 = 40, kF5 = 41, kF6 = 42, kBackspace = 43, kReturn = 55,
  kUp = 58, kShiftR = 59, kAltR = 60, kLeft = 61, kDown = 62, kRight = 63,
  kSpaceL = 68, kSpaceM = 69, kSpaceR = 70, kVolumeUp = 71,
};

// Plain, Shift, AltGr, Shift+AltGr, by bit. 0 falls back (AltGr to plain or
// Shift); a key with 0 plain is handled by number. The AltGr layer is
// badge-bsp's (badge_bsp_input.c) less its combining accents.
constexpr char32_t kMap[kKeyCount][4] = {
    {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},            // esc f1-f3
    {U'`', U'~', 0, 0},     {U'1', U'!', U'¡', U'¹'}, {U'2', U'@', U'²', 0}, {U'3', U'#', U'³', 0},
    {0, 0, 0, 0},           {U'q', U'Q', U'ä', U'Ä'}, {U'w', U'W', U'å', U'Å'}, {U'e', U'E', U'é', U'É'},  // tab q w e
    {0, 0, 0, 0},           {U'a', U'A', U'á', U'Á'}, {U's', U'S', U'ß', U'§'}, {U'd', U'D', U'ð', U'Ð'},  // fn a s d
    {0, 0, 0, 0},           {U'z', U'Z', U'æ', U'Æ'}, {U'x', U'X', U'·', 0},  {U'c', U'C', U'©', U'¢'},   // shift z x c
    {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},           {U'\\', U'|', U'¬', U'¦'}, // ctrl meta alt \ .
    {U'4', U'$', U'¤', U'£'}, {U'5', U'%', U'€', U'¸'}, {U'6', U'^', U'¼', 0}, {U'7', U'&', U'½', 0},
    {U'r', U'R', U'®', U'™'}, {U't', U'T', U'þ', U'Þ'}, {U'y', U'Y', U'ü', U'Ü'}, {U'u', U'U', U'ú', U'Ú'},
    {U'f', U'F', U'ë', U'Ë'}, {U'g', U'G', 0, 0},     {U'h', U'H', 0, 0},     {U'j', U'J', U'ï', U'Ï'},
    {U'v', U'V', 0, 0},     {U'b', U'B', 0, 0},     {U'n', U'N', U'ñ', U'Ñ'}, {U'm', U'M', U'µ', U'±'},
    {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},            // f4-f6 bksp
    {U'9', U'(', U'‘', 0},  {U'0', U')', U'’', 0},  {U'-', U'_', U'¥', 0},  {U'=', U'+', 0, 0},
    {U'o', U'O', U'ó', U'Ó'}, {U'p', U'P', U'ö', U'Ö'}, {U'[', U'{', U'«', U'“'}, {U']', U'}', U'»', U'”'},
    {U'l', U'L', U'ø', U'Ø'}, {U';', U':', 0, 0},     {U'\'', U'"', 0, 0},    {0, 0, 0, 0},            // return
    {U'.', U'>', 0, 0},     {U'/', U'?', U'¿', 0},  {0, 0, 0, 0},           {0, 0, 0, 0},            // up shift
    {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},            // altgr arrows
    {U'8', U'*', U'¾', 0},  {U'i', U'I', U'í', U'Í'}, {U'k', U'K', U'œ', U'Œ'}, {U',', U'<', 0, 0},
    {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},           {0, 0, 0, 0},            // spaces vol
};

bool bit(const uint8_t* s, int b) { return (s[b / 8] >> (b % 8)) & 1; }

bool isModifier(int b) {
  return b == kFn || b == kShiftL || b == kShiftR || b == kCtrl || b == kMeta || b == kAltL ||
         b == kAltR;
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

struct Mods {
  bool shift, fn, altGr, blocked;  // blocked: Ctrl, left Alt or Meta
};

int functionNumber(int b) {
  switch (b) {
    case kF1: return 1;
    case kF2: return 2;
    case kF3: return 3;
    case kF4: return 4;
    case kF5: return 5;
    case kF6: return 6;
    default: return 0;
  }
}

KeyEvent map(int b, const Mods& m) {
  if (const int n = functionNumber(b)) {
    if (!m.fn) return action(functionKeyAction(n));
    switch (n) {
      case 1: return action(functionKeyAction(7));
      case 4: return action(functionKeyAction(10));
      case 5: return action(Action::KbLightStep);
      default: return {};
    }
  }
  switch (b) {
    case kEsc: return action(Action::Back);
    case kReturn: return action(Action::Select);
    case kTab: return action(Action::Palette);
    case kBackspace: return action(Action::Backspace);
    case kUp: case kVolumeUp: return action(Action::Up);
    case kDown: return action(Action::Down);
    case kLeft: return action(Action::Left);
    case kRight: return action(Action::Right);
    case kSpaceL: case kSpaceM: case kSpaceR:
      if (m.fn) return action(Action::Emoji);
      if (m.blocked) return {};
      {
        KeyEvent e = action(Action::Char);
        e.ch = U' ';
        return e;
      }
    default: break;
  }
  const char32_t* d = kMap[b];
  if (!d[0] || m.blocked) return {};
  char32_t c = m.shift ? d[1] : d[0];
  if (m.altGr) {
    const char32_t alt = m.shift ? d[3] : d[2];
    if (alt) c = alt;
  }
  KeyEvent e = action(Action::Char);
  e.ch = c;
  return e;
}

}  // namespace

void TanmatsuKeys::push(const KeyEvent& e) {
  if (e.action == Action::None || count_ == kQueue) return;
  queue_[(head_ + count_) % kQueue] = e;
  ++count_;
}

void TanmatsuKeys::update(const uint8_t state[kTanmatsuKeyBytes], uint32_t ms) {
  const Mods m{bit(state, kShiftL) || bit(state, kShiftR), bit(state, kFn), bit(state, kAltR),
               bit(state, kCtrl) || bit(state, kAltL) || bit(state, kMeta)};
  const bool metaNow = bit(state, kMeta), metaBefore = bit(prev_, kMeta);
  if (metaNow && !metaBefore) metaUsed_ = false;
  const bool ctrlNow = bit(state, kCtrl), ctrlBefore = bit(prev_, kCtrl);
  if (ctrlNow && !ctrlBefore) ctrlUsed_ = false;
  for (int b = 0; b < kKeyCount; ++b) {
    const bool now = bit(state, b), before = bit(prev_, b);
    if (b == heldBit_ && !now) heldBit_ = -1;
    if (!now || before || isModifier(b)) continue;
    if (metaNow) metaUsed_ = true;
    if (ctrlNow) ctrlUsed_ = true;
    const KeyEvent e = map(b, m);
    if (repeats(e.action)) {
      heldBit_ = b;
      heldAction_ = e.action;
      nextRepeatMs_ = ms + kTanmatsuRepeatDelayMs;
    }
    push(e);
  }
  // The logo key tapped on its own goes Home.
  if (!metaNow && metaBefore && !metaUsed_) push(action(Action::OpenHome));
  // Ctrl tapped on its own, as the M9's Ctrl: the emoji tray while typing,
  // the palette elsewhere.
  if (!ctrlNow && ctrlBefore && !ctrlUsed_) push(action(Action::Emoji));
  for (int i = 0; i < kTanmatsuKeyBytes; ++i) prev_[i] = state[i];
}

KeyEvent TanmatsuKeys::next(uint32_t ms) {
  if (count_) {
    const KeyEvent e = queue_[head_];
    head_ = uint8_t((head_ + 1) % kQueue);
    --count_;
    return e;
  }
  if (heldBit_ < 0 || (int32_t)(ms - nextRepeatMs_) < 0) return {};
  nextRepeatMs_ = ms + kTanmatsuRepeatMs;
  return action(heldAction_);
}

}  // namespace rc
