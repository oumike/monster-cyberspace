#include "hal/board.h"

#include <iterator>

// What most boards answer. A board overrides one by defining it in its HAL
// (a strong definition wins over these weak ones).
namespace board {

__attribute__((weak)) bool hasSound() { return true; }
__attribute__((weak)) bool hasVolume() { return false; }
__attribute__((weak)) void setVolume(uint8_t) {}
__attribute__((weak)) bool canType() { return true; }
__attribute__((weak)) bool webCompose() { return false; }
__attribute__((weak)) bool sleepsOnBattery() { return false; }
__attribute__((weak)) bool fixedTextSize(rc::TextSize&) { return false; }
__attribute__((weak)) float batteryVolts() { return 0.0f; }
__attribute__((weak)) void deepSleep(uint64_t) {}
__attribute__((weak)) bool wokeByTimer() { return false; }
__attribute__((weak)) void powerOff() {}
__attribute__((weak)) bool setRedraw(void (*)()) { return false; }
__attribute__((weak)) bool hasHaptic() { return false; }
__attribute__((weak)) void haptic(int) {}
__attribute__((weak)) uint8_t defaultRotation() { return 0; }
__attribute__((weak)) bool hasPowerLight() { return false; }
__attribute__((weak)) void powerLight(uint8_t) {}
__attribute__((weak)) bool hasWifiLight() { return false; }
__attribute__((weak)) void wifiLight(uint8_t) {}
__attribute__((weak)) bool hasLedLevel() { return false; }
__attribute__((weak)) bool keysPeek() { return true; }
__attribute__((weak)) const char32_t* menuKeyName() { return U"ctrl"; }
__attribute__((weak)) void ledLevel(uint8_t) {}
__attribute__((weak)) const rc::FunctionKey* functionKeys(size_t& count) {
  count = std::size(rc::kFunctionKeys);
  return rc::kFunctionKeys;
}

}  // namespace board
