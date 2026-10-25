#pragma once

#include "screens/router.h"
#include "ui/widgets/log_view.h"

namespace rc {

// The room log. Task 19 ships this minimal view (log and scrolling); Task 20
// adds the input line, drawer, palette and message menu.
class RoomScreen : public Screen {
 public:
  void enter(const std::string& slug) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  std::string slug_;
  LogView log_;
  uint32_t version_ = UINT32_MAX;
  int cols_ = 0;
};

}  // namespace rc
