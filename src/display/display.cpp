#include "display.h"
#include "config/config.h"

// Define the global display instance
LGFX gfx;

// LGFX class constructor - configures display for CYD hardware
LGFX::LGFX(void)
{
  {
    auto cfg = _bus_instance.config();
    cfg.spi_host = HSPI_HOST;      // CRITICAL: CYD uses HSPI not VSPI!
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read  = 16000000;
    cfg.spi_3wire  = false;
    cfg.use_lock   = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = TFT_SCK;
    cfg.pin_mosi = TFT_MOSI;
    cfg.pin_miso = -1;
    cfg.pin_dc   = TFT_DC;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
  }

  {
    auto cfg = _panel_instance.config();
    cfg.pin_cs           = TFT_CS;
    cfg.pin_rst          = TFT_RST;
    cfg.pin_busy         = -1;
    cfg.memory_width     = 320;
    cfg.memory_height    = 480;
    cfg.panel_width      = 320;
    cfg.panel_height     = 480;
    cfg.offset_x         = 0;
    cfg.offset_y         = 0;
    cfg.offset_rotation  = 0;
    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits  = 1;
    cfg.readable         = true;
    cfg.invert           = true;        // CYD needs inversion
    cfg.rgb_order        = true;        // CYD uses BGR
    cfg.dlen_16bit       = false;
    cfg.bus_shared       = true;        // Shared with touch
    _panel_instance.config(cfg);
  }

  {
    auto cfg = _light_instance.config();
    cfg.pin_bl = TFT_BL;      // GPIO27
    cfg.invert = false;
    cfg.freq   = 44100;
    cfg.pwm_channel = 1;
    _light_instance.config(cfg);
    _panel_instance.setLight(&_light_instance);
  }

  setPanel(&_panel_instance);
}

void showSplashScreen() {
  gfx.setTextColor(COLOR_HEADER);
  gfx.setTextSize(3);
  gfx.setCursor(80, 120);
  gfx.println("FluidDash");
  gfx.setTextSize(2);
  gfx.setCursor(140, 160);
  gfx.println("v0.7");
  gfx.setCursor(160, 190);
  gfx.println("Initializing...");
}

void setBrightness(uint8_t brightness) {
  gfx.setBrightness(brightness);
}
