#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/panel_rm69a10.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32p4/Bus_DSI.hpp>
#include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>

#include "hal/tdisplay_p4/io_tdisplay_p4.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"
#include "ui/scale_blit.h"

namespace p4panel {
namespace {

using namespace tdisplay_p4;

class Panel : public lgfx::Panel_DSI {
 public:
  bool setDcsBrightness(uint8_t b) { return write_params(0x51, &b, 1); }

 protected:
  const uint8_t* getInitParams(size_t listno) const override {
    static constexpr uint8_t list0[] = {
        2, 0xFE, 0xFD,  //
        2, 0x80, 0xFC,  //
        2, 0xFE, 0x00,  //
        5, 0x2A, 0x00, 0x00, 0x02, 0x37,  //
        5, 0x2B, 0x00, 0x00, 0x04, 0xCF,  //
        5, 0x31, 0x00, 0x03, 0x02, 0x34,  //
        5, 0x30, 0x00, 0x00, 0x04, 0xCF,  //
        2, 0x12, 0x00,  //
        2, 0x35, 0x00,  //
        2, 0x51, 0x00,  //
        1, CMD_SLPOUT,  //
        0,
    };
    static constexpr uint8_t list1[] = {1, CMD_DISPON, 0};
    switch (listno) {
      case 0: return list0;
      case 1: return list1;
      default: return nullptr;
    }
  }
  size_t getInitDelay(size_t listno) const override { return listno == 0 ? 120 : 0; }
};

class Device : public lgfx::LGFX_Device {
  lgfx::Bus_DSI bus_;

 public:
  Panel panel;

  Device() {
    {
      auto cfg = bus_.config();
      cfg.bus_id = 0;
      cfg.lane_num = DSI_LANES;
      cfg.lane_mbps = DSI_LANE_MBPS;
      cfg.ldo_chan_id = 3;
      cfg.ldo_voltage_mv = 2500;
      bus_.config(cfg);
    }
    {
      auto cfg = panel.config();
      cfg.memory_width = cfg.panel_width = PANEL_W;
      cfg.memory_height = cfg.panel_height = PANEL_H;
      cfg.readable = true;
      cfg.rgb_order = true;
      cfg.bus_shared = false;
      cfg.pin_cs = -1;
      cfg.pin_rst = -1;
      panel.config(cfg);
      panel.setBus(&bus_);
      auto d = panel.config_detail();
      d.dpi_freq_mhz = DSI_DPI_MHZ;
      d.hsync_back_porch = DSI_HBP;
      d.hsync_pulse_width = DSI_HSYNC;
      d.hsync_front_porch = DSI_HFP;
      d.vsync_back_porch = DSI_VBP;
      d.vsync_pulse_width = DSI_VSYNC;
      d.vsync_front_porch = DSI_VFP;
      panel.config_detail(d);
    }
    setPanel(&panel);
  }

  // The reset line is on the expander.
  bool init_impl(bool use_reset, bool use_clear) override {
    if (!ioSet(EXP_SCREEN_RST, false)) return false;
    delay(10);
    if (!ioSet(EXP_SCREEN_RST, true)) return false;
    delay(120);
    return lgfx::LGFX_Device::init_impl(use_reset, use_clear);
  }
};

Device lcd;
bool ok = false;

// Frame rows per pass, and the panel pixels they make.
constexpr int kBand = 16;
uint16_t* buf = nullptr;

}  // namespace

bool begin() {
  ok = lcd.init();
  if (!ok) {
    Serial.println("[p4panel] DSI init failed");
    return false;
  }
  lcd.setRotation(0);
  lcd.fillScreen(0);
  const size_t n = size_t(kBand) * (PANEL_H / SCALE) * SCALE * SCALE;
  buf = static_cast<uint16_t*>(heap_caps_malloc(n * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!buf) buf = static_cast<uint16_t*>(heap_caps_malloc(n * 2, MALLOC_CAP_SPIRAM));
  return buf != nullptr;
}

void pushRows(const uint16_t* frame, int frameW, int frameH, int y0, int y1, uint8_t r) {
  if (!ok || !buf) return;
  lcd.startWrite();
  for (int y = y0; y < y1; y += kBand) {
    const int e = y + kBand < y1 ? y + kBand : y1;
    const rc::BlitRect b = rc::scaleBlitRows(frame, frameW, frameH, y, e, r, SCALE, buf);
    lcd.pushImage(b.x, b.y, b.w, b.h, reinterpret_cast<const lgfx::rgb565_t*>(buf));
  }
  lcd.endWrite();
}

void brightness(uint8_t level) {
  if (ok) lcd.panel.setDcsBrightness(level);
}

void clear() {
  if (ok) lcd.fillScreen(0);
}

}  // namespace p4panel

#endif
