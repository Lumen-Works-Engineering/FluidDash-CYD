#ifndef PINS_H
#define PINS_H

// ========== CYD HARDWARE PIN CONFIGURATION ==========
// Compatible with E32R35T (3.5") and E32R40T (4.0")
// Based on LCD Wiki official documentation - see FLUIDDASH_HARDWARE_REFERENCE.md

// Display & Touch (Pre-wired onboard - HSPI bus)
#define TFT_CS      15    // LCD chip select
#define TFT_DC      2     // Data/command
#define TFT_RST     -1    // Shared with EN button (hardware reset)
#define TFT_MOSI    13    // SPI MOSI (HSPI)
#define TFT_SCK     14    // SPI clock (HSPI)
#define TFT_MISO    12    // SPI MISO (HSPI)
#define TFT_BL      27    // Backlight control
#define TOUCH_CS    33    // Touch chip select
#define TOUCH_IRQ   36    // Touch interrupt

// FluidDash Sensors (External connections via connectors)
#define ONE_WIRE_BUS_1    21    // Internal motor drivers (P3 SPI_CS pin)
#define RTC_SDA           32    // I2C connector (P4)
#define RTC_SCL           25    // I2C connector (P4)
#define FAN_PWM           4     // Fan PWM control (repurpose AUDIO_EN)
#define FAN_TACH          35    // Fan tachometer (P2 expansion pin)
#define PSU_VOLT          34    // PSU voltage monitor (repurpose BAT_ADC)

// RGB Status LED (Pre-wired onboard - common anode, LOW=on)
#define LED_RED     22
#define LED_GREEN   16
#define LED_BLUE    17

// Mode button (use GPIO0 - BOOT button)
#define BTN_MODE    0

// SD Card (VSPI bus)
#define SD_CS    5
#define SD_MOSI  23
#define SD_SCK   18
#define SD_MISO  19

// Constants
#define PWM_FREQ     25000
#define PWM_RESOLUTION 8
#define SERIES_RESISTOR 10000
#define THERMISTOR_NOMINAL 10000
#define TEMPERATURE_NOMINAL 25
#define B_COEFFICIENT 3950
#define ADC_RESOLUTION 4095.0
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define WDT_TIMEOUT 10  // Watchdog timeout in seconds

// Colors (RGB565 format)
#define COLOR_BG       0x0000  // Black
#define COLOR_HEADER   0x001F  // Blue
#define COLOR_TEXT     0xFFFF  // White
#define COLOR_VALUE    0x07FF  // Cyan
#define COLOR_WARN     0xF800  // Red
#define COLOR_GOOD     0x07E0  // Green
#define COLOR_LINE     0x4208  // Dark Gray
#define COLOR_ORANGE   0xFD20  // Orange
#define COLOR_YELLOW   0xFFE0  // Yellow
#define COLOR_PURPLE   0xF81F  // Magenta/Purple

#endif // PINS_H
