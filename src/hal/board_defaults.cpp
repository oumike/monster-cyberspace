#include "hal/board.h"

// What most boards answer. A board overrides one by defining it in its HAL
// (a strong definition wins over these weak ones).
namespace board {

__attribute__((weak)) bool canType() { return true; }
__attribute__((weak)) bool webCompose() { return false; }
__attribute__((weak)) bool sleepsOnBattery() { return false; }
__attribute__((weak)) float batteryVolts() { return 0.0f; }
__attribute__((weak)) void deepSleep(uint64_t) {}
__attribute__((weak)) bool wokeByTimer() { return false; }
__attribute__((weak)) void powerOff() {}
__attribute__((weak)) bool setRedraw(void (*)()) { return false; }
__attribute__((weak)) int uiScale() { return 1; }

}  // namespace board
