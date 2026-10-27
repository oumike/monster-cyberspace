#if defined(BOARD_CROWPANEL_35)

#include "hal/crowpanel/panel_ili9488.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "hal/crowpanel/pins_crowpanel.h"

// Bus and panel settings from camillia-mt's display.h (Meshtastic's
// elecrow-adv-35-tft flags).

namespace ili9488 {
namespace {

using namespace crowpanel::p35;

class Device : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 panel_;
  lgfx::Bus_SPI bus_;

 public:
  Device() {
    {
      auto cfg = bus_.config();
      // SPI3: SPI2 is left to the SD card.
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = TFT_WRITE_HZ;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = TFT_SCK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = TFT_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.panel_width = PANEL_NATIVE_W;
      cfg.panel_height = PANEL_NATIVE_H;
      cfg.memory_width = PANEL_NATIVE_W;
      cfg.memory_height = PANEL_NATIVE_H;
      // LovyanGFX rotation 3 is landscape the right way up (camillia-mt,
      // checked on the hardware), so setRotation(0) lands there.
      cfg.offset_rotation = 3;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.readable = false;
      panel_.config(cfg);
    }
    setPanel(&panel_);
  }
};

Device lcd;

}  // namespace

bool begin() {
  bool ok = lcd.init();
  lcd.setRotation(0);
  lcd.fillScreen(0);
  return ok;
}

void pushRows(const uint16_t* frame, int width, int y0, int y1) {
  if (y1 <= y0) return;
  lcd.startWrite();
  lcd.pushImage(0, y0, width, y1 - y0,
                reinterpret_cast<const lgfx::rgb565_t*>(frame + (size_t)y0 * width));
  lcd.endWrite();
}

void setRotation(uint8_t r) { lcd.setRotation(r & 3); }

void sleep(bool on) {
  if (on) lcd.sleep();
  else lcd.wakeup();
}

}  // namespace ili9488

#endif
