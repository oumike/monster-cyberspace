#pragma once

#include <cstdint>

namespace rc {

// Raises the setup AP when the saved networks have failed for a while, so a
// device whose Wi-Fi went away can still be reached at 192.168.4.1. An AP the
// fallback raised drops again once the station reconnects and nobody is on
// it. An AP the user turned on stays until they turn it off.
class ApFallback {
 public:
  static constexpr int64_t kAfterMs = 5 * 60 * 1000;
  enum class Act { None, Raise, Drop };

  // Call often. `apClients` is the number of stations joined to the AP.
  Act update(int64_t nowMs, bool connected, bool apUp, int apClients);
  // The user turned the AP on or off. Off restarts the timer.
  void noteManual(int64_t nowMs, bool apOn);

 private:
  bool started_ = false, auto_ = false;
  int64_t lastUp_ = 0;
};

}  // namespace rc
