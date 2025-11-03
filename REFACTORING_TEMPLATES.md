# Example Refactored File Templates

These examples show what the refactored files should look like.

## Example 1: pins.h

```cpp
#ifndef PINS_H
#define PINS_H

// ========== CYD HARDWARE PIN CONFIGURATION ==========
// Compatible with E32R35T (3.5") and E32R40T (4.0")

// Display & Touch (Pre-wired onboard - HSPI bus)
#define TFT_CS      15
#define TFT_DC      2
#define TFT_RST     -1
#define TFT_MOSI    13
#define TFT_SCK     14
#define TFT_MISO    12
#define TFT_BL      27
#define TOUCH_CS    33
#define TOUCH_IRQ   36

// FluidDash Sensors
#define ONE_WIRE_BUS_1    21
#define RTC_SDA           32
#define RTC_SCL           25
#define FAN_PWM           4
#define FAN_TACH          35
#define PSU_VOLT          34

// RGB Status LED
#define LED_RED     22
#define LED_GREEN   16
#define LED_BLUE    17

// Mode button
#define BTN_MODE    0

// SD Card
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
#define WDT_TIMEOUT 10

// Colors
#define COLOR_BG       0x0000
#define COLOR_HEADER   0x001F
#define COLOR_TEXT     0xFFFF
#define COLOR_VALUE    0x07FF
#define COLOR_WARN     0xF800
#define COLOR_GOOD     0x07E0
#define COLOR_LINE     0x4208
#define COLOR_ORANGE   0xFD20

#endif // PINS_H
```

## Example 2: config.h

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Display Modes
enum DisplayMode {
  MODE_MONITOR,
  MODE_ALIGNMENT,
  MODE_GRAPH,
  MODE_NETWORK
};

// Element types for JSON-defined screens
enum ElementType {
    ELEM_NONE = 0,
    ELEM_RECT,
    ELEM_LINE,
    ELEM_TEXT_STATIC,
    ELEM_TEXT_DYNAMIC,
    ELEM_TEMP_VALUE,
    ELEM_COORD_VALUE,
    ELEM_STATUS_VALUE,
    ELEM_PROGRESS_BAR,
    ELEM_GRAPH
};

// Alignment options
enum TextAlign {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

// Screen element definition
struct ScreenElement {
    ElementType type;
    int16_t x, y, w, h;
    uint16_t color;
    uint16_t bgColor;
    uint8_t textSize;
    char label[32];
    char dataSource[32];
    uint8_t decimals;
    bool filled;
    TextAlign align;
    bool showLabel;
};

// Screen layout definition
struct ScreenLayout {
    char name[32];
    uint16_t backgroundColor;
    ScreenElement elements[60];
    uint8_t elementCount;
    bool isValid;
};

// Configuration Structure
struct Config {
  // Network
  char device_name[32];
  char fluidnc_ip[16];
  uint16_t fluidnc_port;
  bool fluidnc_auto_discover;
  
  // Temperature
  float temp_threshold_low;
  float temp_threshold_high;
  float temp_offset_x;
  float temp_offset_yl;
  float temp_offset_yr;
  float temp_offset_z;
  
  // Fan Control
  uint8_t fan_min_speed;
  uint8_t fan_max_speed_limit;
  
  // PSU Monitoring
  float psu_voltage_cal;
  float psu_alert_low;
  float psu_alert_high;
  
  // Display Settings
  uint8_t brightness;
  DisplayMode default_mode;
  bool show_machine_coords;
  bool show_temp_graph;
  uint8_t coord_decimal_places;
  
  // Graph Settings
  uint16_t graph_timespan_seconds;
  uint16_t graph_update_interval;
  
  // Units
  bool use_fahrenheit;
  bool use_inches;
  
  // Advanced
  bool enable_logging;
  uint16_t status_update_rate;
};

// Global config instance
extern Config cfg;

// Function declarations
void initDefaultConfig();
void loadConfig();
void saveConfig();

#endif // CONFIG_H
```

## Example 3: config.cpp

```cpp
#include "config.h"
#include <Preferences.h>

// Define the global config instance
Config cfg;
Preferences prefs;

void initDefaultConfig() {
  strcpy(cfg.device_name, "fluiddash");
  strcpy(cfg.fluidnc_ip, "192.168.73.13");
  cfg.fluidnc_port = 81;
  cfg.fluidnc_auto_discover = true;

  cfg.temp_threshold_low = 30.0;
  cfg.temp_threshold_high = 50.0;
  // ... rest of defaults
}

void loadConfig() {
  initDefaultConfig();  // Set defaults first
  
  if (!prefs.begin("fluiddash", true)) {
    return;  // Use defaults if can't read
  }

  // Load from preferences
  prefs.getString("device_name", cfg.device_name, sizeof(cfg.device_name));
  // ... rest of loading
  
  prefs.end();
}

void saveConfig() {
  if (!prefs.begin("fluiddash", false)) {
    return;
  }

  // Save to preferences
  prefs.putString("device_name", cfg.device_name);
  // ... rest of saving
  
  prefs.end();
}
```

## Example 4: display.h

```cpp
#ifndef DISPLAY_H
#define DISPLAY_H

#include <LovyanGFX.hpp>
#include "config/config.h"
#include "config/pins.h"

// LovyanGFX Display Configuration for ST7796 480x320
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void);
};

// Global display instance
extern LGFX gfx;

// Display control functions
void initDisplay();
void showSplashScreen();
void setBrightness(uint8_t brightness);

#endif // DISPLAY_H
```

## Example 5: sensors.h

```cpp
#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// Temperature globals
extern float temperatures[4];
extern float peakTemps[4];
extern float *tempHistory;
extern uint16_t historySize;
extern uint16_t historyIndex;

// Fan control globals
extern volatile uint16_t tachCounter;
extern uint16_t fanRPM;
extern uint8_t fanSpeed;

// PSU monitoring globals
extern float psuVoltage;
extern float psuMin;
extern float psuMax;

// ADC sampling
extern uint32_t adcSamples[5][10];
extern uint8_t adcSampleIndex;
extern uint8_t adcCurrentSensor;
extern unsigned long lastAdcSample;
extern bool adcReady;

// Function declarations
void initSensors();
void sampleSensorsNonBlocking();
void processAdcReadings();
float calculateThermistorTemp(float adcValue);
void readTemperatures();
void updateTempHistory();
void allocateHistoryBuffer();

// Fan control
void initFan();
void controlFan();
void calculateRPM();
void IRAM_ATTR tachISR();

// PSU monitoring
void initPSU();
float readPSUVoltage();

#endif // SENSORS_H
```

## Example 6: main.cpp (After Refactoring)

```cpp
#include <Arduino.h>

// Project modules
#include "config/pins.h"
#include "config/config.h"
#include "display/display.h"
#include "display/screen_renderer.h"
#include "display/ui_modes.h"
#include "sensors/sensors.h"
#include "network/wifi_manager.h"
#include "network/web_server.h"
#include "network/fluidnc_client.h"
#include "storage/sd_card.h"
#include "utils/rtc.h"
#include "utils/watchdog.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nFluidDash CYD Edition v0.1");
  
  // Load configuration
  loadConfig();
  
  // Initialize watchdog
  enableLoopWDT();
  
  // Initialize display
  initDisplay();
  showSplashScreen();
  
  // Initialize hardware
  initSensors();
  initFan();
  initPSU();
  initRTC();
  initSDCard();
  
  // Setup networking
  setupWiFiManager();
  setupWebServer();
  
  // Connect to FluidNC
  if (cfg.fluidnc_auto_discover) {
    discoverFluidNC();
  }
  connectFluidNC();
  
  // Load screen layouts from SD
  loadScreenLayouts();
  
  // Initialize display mode
  setDisplayMode(cfg.default_mode);
  
  Serial.println("Setup complete!");
}

void loop() {
  feedLoopWDT();
  
  // Network processing
  webSocketLoop();
  
  // Sensor sampling (non-blocking)
  sampleSensorsNonBlocking();
  
  // Process sensor readings when ready
  if (adcReady) {
    processAdcReadings();
    readTemperatures();
    controlFan();
  }
  
  // Update display
  updateDisplay();
  
  // Handle button input
  handleButton();
  
  // Periodic tasks
  static unsigned long lastTempHistoryUpdate = 0;
  if (millis() - lastTempHistoryUpdate > cfg.graph_update_interval * 1000) {
    updateTempHistory();
    lastTempHistoryUpdate = millis();
  }
  
  static unsigned long lastRPMCalculation = 0;
  if (millis() - lastRPMCalculation > 1000) {
    calculateRPM();
    lastRPMCalculation = millis();
  }
}
```

## Key Observations

### Benefits of Refactored Structure:
1. **main.cpp is ~80 lines** - easy to understand the program flow
2. **Each module has a clear purpose** - temperature, display, network, etc.
3. **Easy to find and fix bugs** - know exactly where to look
4. **Easy to add features** - just extend the relevant module
5. **Can test modules independently** - write unit tests later
6. **Multiple people can work on different modules** - less conflicts

### Pattern to Follow:
```
Module Header (.h):
  - Include guards
  - Necessary includes (minimize)
  - Type definitions (enums, structs)
  - extern declarations for globals
  - Function declarations
  
Module Implementation (.cpp):
  - Include own header first
  - Include other necessary headers
  - Define global variables
  - Implement functions
```

### Important Notes:
- Each .cpp file that uses Serial must include `<Arduino.h>`
- Each .cpp file that uses gfx must include `"display/display.h"`
- Each .cpp file that accesses cfg must include `"config/config.h"`
- ISR functions must be marked with `IRAM_ATTR`
- Don't forget to define extern variables in exactly ONE .cpp file

## File Size Guidelines
After refactoring, typical file sizes should be:
- **Headers (.h)**: 50-150 lines
- **Implementation (.cpp)**: 100-400 lines
- **main.cpp**: 80-200 lines

If any file exceeds 500 lines, consider breaking it into smaller modules.
