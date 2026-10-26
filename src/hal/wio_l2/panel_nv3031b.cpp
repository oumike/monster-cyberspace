#if defined(BOARD_WIO_L2)

#include "hal/wio_l2/panel_nv3031b.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/panel/Panel_NV3031B.hpp>

#include "hal/wio_l2/pins_wio_l2.h"

// Bus and panel settings from camillia-mt's display.h and platy-aprs's
// display.cpp (both from Seeed's LovyanGFX configuration).

namespace nv3031b {
namespace {

class Device : public lgfx::LGFX_Device {
  lgfx::Panel_NV3031B panel_;
  lgfx::Bus_SPI bus_;

 public:
  Device() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 3;
      cfg.freq_write = wio::TFT_WRITE_HZ;
      cfg.freq_read = 16000000;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = wio::TFT_SCK;
      cfg.pin_mosi = -1;
      cfg.pin_miso = -1;
      cfg.pin_dc = -1;  // no DC line: command/data is in the QSPI transaction
      // Setting all four IO pins is what selects quad mode.
      cfg.pin_io0 = wio::TFT_IO0;
      cfg.pin_io1 = wio::TFT_IO1;
      cfg.pin_io2 = wio::TFT_IO2;
      cfg.pin_io3 = wio::TFT_IO3;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = wio::TFT_CS;
      cfg.pin_rst = -1;  // reset is on the expander, done in ioBegin()
      cfg.pin_busy = -1;
      cfg.panel_width = wio::PANEL_NATIVE_W;
      cfg.panel_height = wio::PANEL_NATIVE_H;
      cfg.memory_width = wio::PANEL_NATIVE_W;
      cfg.memory_height = wio::PANEL_NATIVE_H;
      cfg.offset_rotation = 1;  // with setRotation(0): landscape 320x240
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      cfg.invert = true;
      cfg.rgb_order = true;
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
  // rgb565_t is host order; LovyanGFX swaps to the panel's byte order.
  lcd.pushImage(0, y0, width, y1 - y0,
                reinterpret_cast<const lgfx::rgb565_t*>(frame + (size_t)y0 * width));
  lcd.endWrite();
}

void sleep(bool on) {
  if (on) lcd.sleep();
  else lcd.wakeup();
}

}  // namespace nv3031b

#endif
