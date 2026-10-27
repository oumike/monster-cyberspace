#if defined(BOARD_CROWPANEL_50)

#include "hal/crowpanel/panel_rgb800.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

#include "hal/crowpanel/pins_crowpanel.h"

// LovyanGFX's RGB bus, set up as Elecrow's V1.2/V1.3 examples and camillia-mt's
// crowpanel_50_display.h do. ESP-IDF's esp_lcd RGB driver, with what should be
// the same timings, only got the panel's self-test colour cycle.

namespace rgb800 {
namespace {

using namespace crowpanel::p50;

class Device : public lgfx::LGFX_Device {
  lgfx::Bus_RGB bus_;
  lgfx::Panel_RGB panel_;

 public:
  Device() {
    {
      auto cfg = panel_.config();
      cfg.memory_width = PANEL_W;
      cfg.memory_height = PANEL_H;
      cfg.panel_width = PANEL_W;
      cfg.panel_height = PANEL_H;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      panel_.config(cfg);
    }
    {
      auto cfg = panel_.config_detail();
      cfg.use_psram = 1;  // 800x480x2 = 750 KB
      panel_.config_detail(cfg);
    }
    {
      auto cfg = bus_.config();
      cfg.panel = &panel_;
      cfg.pin_d0 = PIN_DATA[0];
      cfg.pin_d1 = PIN_DATA[1];
      cfg.pin_d2 = PIN_DATA[2];
      cfg.pin_d3 = PIN_DATA[3];
      cfg.pin_d4 = PIN_DATA[4];
      cfg.pin_d5 = PIN_DATA[5];
      cfg.pin_d6 = PIN_DATA[6];
      cfg.pin_d7 = PIN_DATA[7];
      cfg.pin_d8 = PIN_DATA[8];
      cfg.pin_d9 = PIN_DATA[9];
      cfg.pin_d10 = PIN_DATA[10];
      cfg.pin_d11 = PIN_DATA[11];
      cfg.pin_d12 = PIN_DATA[12];
      cfg.pin_d13 = PIN_DATA[13];
      cfg.pin_d14 = PIN_DATA[14];
      cfg.pin_d15 = PIN_DATA[15];
      cfg.pin_henable = PIN_DE;
      cfg.pin_vsync = PIN_VSYNC;
      cfg.pin_hsync = PIN_HSYNC;
      cfg.pin_pclk = PIN_PCLK;
      cfg.freq_write = PCLK_HZ;
      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch = 8;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch = 8;
      cfg.pclk_idle_high = 1;
      bus_.config(cfg);
    }
    panel_.setBus(&bus_);
    setPanel(&panel_);
  }
};

Device lcd;

// Our quarter turns go clockwise from landscape; LovyanGFX's the other way:
// frame (x, y) at our r1 lands on panel (y, H-1-x), LovyanGFX's rotation 3.
uint8_t lgfxRotation(uint8_t r) { return (4 - (r & 3)) & 3; }

}  // namespace

bool begin() {
  const bool ok = lcd.init();
  if (!ok) Serial.println("[rgb800] LovyanGFX init failed");
  lcd.setRotation(0);
  lcd.fillScreen(0);
  return ok;
}

void clear() { lcd.fillScreen(0); }

void pushRows(const uint16_t* frame, int frameW, int y0, int y1, uint8_t r) {
  if (y1 <= y0) return;
  const uint8_t want = lgfxRotation(r);
  if (lcd.getRotation() != want) lcd.setRotation(want);
  lcd.startWrite();
  lcd.pushImage(0, y0, frameW, y1 - y0,
                reinterpret_cast<const lgfx::rgb565_t*>(frame + (size_t)y0 * frameW));
  lcd.endWrite();
}

}  // namespace rgb800

#endif
