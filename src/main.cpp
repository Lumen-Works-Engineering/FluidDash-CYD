/*
 * FluidDash v0.09 - CYD Edition
 * Configured for ESP32-2432S028 (CYD 3.5" or 4.0" modules)
 * - WiFiManager for initial setup
 * - Preferences for persistent storage
 * - Web interface for all settings
 * - Configurable graph timespan
 */

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#if __has_include(<WebSocketsClient.h>)
#include <WebSocketsClient.h>
#else
enum WStype_t {
  WStype_DISCONNECTED,
  WStype_CONNECTED,
  WStype_TEXT,
  WStype_ERROR
};

class WebSocketsClient {
public:
  WebSocketsClient() {}
  // Begin with host (c-string), port and path
  void begin(const char* host, uint16_t port, const char* path) {}
  // Register event callback: void callback(WStype_t type, uint8_t* payload, size_t length)
  void onEvent(void (*cb)(WStype_t, uint8_t*, size_t)) {}
  void setReconnectInterval(unsigned long) {}
  // Send text - return true on pretend success
  bool sendTXT(const String& /*txt*/) { return true; }
  // Called in loop to perform processing (no-op in stub)
  void loop() {}
};

#endif

#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

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

// Display Modes
enum DisplayMode {
  MODE_MONITOR,
  MODE_ALIGNMENT,
  MODE_GRAPH,
  MODE_NETWORK
};

// Configuration Structure
struct Config {
  // Network
  char device_name[32];
  char fluidnc_ip[16];
  uint16_t fluidnc_port;
  bool fluidnc_auto_discover;
  
  // Temperature - User Settings
  float temp_threshold_low;
  float temp_threshold_high;
  
  // Temperature - Admin Calibration
  float temp_offset_x;
  float temp_offset_yl;
  float temp_offset_yr;
  float temp_offset_z;
  
  // Fan Control
  uint8_t fan_min_speed;
  uint8_t fan_max_speed_limit;  // Safety limit
  
  // PSU Monitoring
  float psu_voltage_cal;
  float psu_alert_low;
  float psu_alert_high;
  
  // Display Settings
  uint8_t brightness;
  DisplayMode default_mode;
  bool show_machine_coords;
  bool show_temp_graph;
  uint8_t coord_decimal_places;  // 2 or 3
  
  // Graph Settings
  uint16_t graph_timespan_seconds;  // 60 to 3600 (1-60 minutes)
  uint16_t graph_update_interval;    // How often to add point (1-60 seconds)
  
  // Units
  bool use_fahrenheit;
  bool use_inches;
  
  // Advanced
  bool enable_logging;
  uint16_t status_update_rate;  // FluidNC polling rate (ms)
};

// Default configuration
Config cfg;

void initDefaultConfig() {
  strcpy(cfg.device_name, "fluiddash");
  strcpy(cfg.fluidnc_ip, "192.168.73.13");
  cfg.fluidnc_port = 81;  // FluidNC WebSocket default port
  cfg.fluidnc_auto_discover = true;

  cfg.temp_threshold_low = 30.0;
  cfg.temp_threshold_high = 50.0;

  cfg.temp_offset_x = 0.0;
  cfg.temp_offset_yl = 0.0;
  cfg.temp_offset_yr = 0.0;
  cfg.temp_offset_z = 0.0;
  cfg.fan_min_speed = 30;
  cfg.fan_max_speed_limit = 100;

  cfg.psu_voltage_cal = 7.3;
  cfg.psu_alert_low = 23.0;
  cfg.psu_alert_high = 25.0;

  cfg.brightness = 255;
  cfg.default_mode = MODE_MONITOR;
  cfg.show_machine_coords = true;
  cfg.show_temp_graph = true;
  cfg.coord_decimal_places = 2;

  cfg.graph_timespan_seconds = 300;  // 5 minutes default
  cfg.graph_update_interval = 5;      // 5 seconds per point

  cfg.use_fahrenheit = true;
  cfg.use_inches = false;

  cfg.enable_logging = false;
  cfg.status_update_rate = 200;
}

// LovyanGFX Display Configuration for ST7796 480x320
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void)
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
};

LGFX gfx;
RTC_DS3231 rtc;
WebSocketsClient webSocket;
Preferences prefs;
AsyncWebServer server(80);
WiFiManager wm;

// Runtime variables
DisplayMode currentMode;
bool sdCardAvailable = false;
volatile uint16_t tachCounter = 0;
uint16_t fanRPM = 0;
uint8_t fanSpeed = 0;
float temperatures[4] = {0};
float peakTemps[4] = {0};
float psuVoltage = 0;
float psuMin = 99.9;
float psuMax = 0.0;

// Non-blocking ADC sampling
uint32_t adcSamples[5][10];  // 4 thermistors + 1 PSU, 10 samples each
uint8_t adcSampleIndex = 0;
uint8_t adcCurrentSensor = 0;
unsigned long lastAdcSample = 0;
bool adcReady = false;

// Dynamic history buffer
float *tempHistory = nullptr;
uint16_t historySize = 0;
uint16_t historyIndex = 0;

// FluidNC status
String machineState = "OFFLINE";
float posX = 0, posY = 0, posZ = 0, posA = 0;
float wposX = 0, wposY = 0, wposZ = 0, wposA = 0;
int feedRate = 0;
int spindleRPM = 0;
bool fluidncConnected = false;
unsigned long jobStartTime = 0;
bool isJobRunning = false;

// ===== ADD NEW GLOBAL VARIABLES HERE =====
// Extended status fields
int feedOverride = 100;
int rapidOverride = 100;
int spindleOverride = 100;
float wcoX = 0, wcoY = 0, wcoZ = 0, wcoA = 0;

// WebSocket reporting
bool autoReportingEnabled = false;
unsigned long reportingSetupTime = 0;

// Debug control
bool debugWebSocket = false;  // Set to true only when debugging
// ===== END NEW GLOBAL VARIABLES =====

// WiFi AP mode flag
bool inAPMode = false;

// RTC availability flag
bool rtcAvailable = false;

// Timing
unsigned long lastTachRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastHistoryUpdate = 0;
unsigned long lastStatusRequest = 0;
unsigned long sessionStartTime = 0;
unsigned long buttonPressStart = 0;
bool buttonPressed = false;

// Colors
#define COLOR_BG       0x0000
#define COLOR_HEADER   0x001F
#define COLOR_TEXT     0xFFFF
#define COLOR_VALUE    0x07FF
#define COLOR_WARN     0xF800
#define COLOR_GOOD     0x07E0
#define COLOR_LINE     0x4208
#define COLOR_ORANGE   0xFD20

// ========== PHASE 2: JSON SCREEN LAYOUT SYSTEM ==========

// Element types for JSON-defined screens
enum ElementType {
    ELEM_NONE = 0,
    ELEM_RECT,              // Filled or outline rectangle
    ELEM_LINE,              // Horizontal or vertical line
    ELEM_TEXT_STATIC,       // Fixed label text
    ELEM_TEXT_DYNAMIC,      // Text from data source
    ELEM_TEMP_VALUE,        // Temperature display (temp0-3)
    ELEM_COORD_VALUE,       // Coordinate display (posX, wposX, etc)
    ELEM_STATUS_VALUE,      // Status text (machineState, feedRate, etc)
    ELEM_PROGRESS_BAR,      // Progress bar (for job completion)
    ELEM_GRAPH              // Mini graph placeholder
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
    char label[32];          // For static text or prefix (e.g., "X:")
    char dataSource[32];     // Data source identifier (e.g., "wposX", "temp0")
    uint8_t decimals;        // Decimal places for numeric values
    bool filled;             // For rectangles - filled or outline
    TextAlign align;         // Text alignment
    bool showLabel;          // Show label prefix
};

// Screen layout definition
struct ScreenLayout {
    char name[32];
    uint16_t backgroundColor;
    ScreenElement elements[60];  // Max 60 elements per screen
    uint8_t elementCount;
    bool isValid;
};

// ========== END PHASE 2 STRUCTURES ==========
// ========== PHASE 2: JSON LAYOUT STORAGE ==========
ScreenLayout monitorLayout;
ScreenLayout alignmentLayout;
ScreenLayout graphLayout;
ScreenLayout networkLayout;
bool layoutsLoaded = false;
// ========== END PHASE 2 GLOBALS ==========

// ========== Function Prototypes ==========
void loadConfig();
void saveConfig();
void allocateHistoryBuffer();
void setupWiFiManager();
void setupWebServer();
String getMainHTML();
String getSettingsHTML();
String getAdminHTML();
String getWiFiConfigHTML();
String getConfigJSON();
String getStatusJSON();
void connectFluidNC();
void discoverFluidNC();
void fluidNCWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void parseFluidNCStatus(String status);
void sampleSensorsNonBlocking();
void processAdcReadings();
float calculateThermistorTemp(float adcValue);
void readTemperatures();
void controlFan();
void calculateRPM();
void updateTempHistory();
void drawScreen();
void drawMonitorMode();
void updateDisplay();
void updateMonitorMode();
void drawAlignmentMode();
void updateAlignmentMode();
void drawGraphMode();
void updateGraphMode();
void drawNetworkMode();
void updateNetworkMode();
void drawTempGraph(int x, int y, int w, int h);
void handleButton();
void cycleDisplayMode();
void showHoldProgress();
void enterSetupMode();
void showSplashScreen();
const char* getMonthName(int month);
void enableLoopWDT();
void feedLoopWDT();
// ========== PHASE 2: JSON FUNCTION PROTOTYPES ==========
bool loadScreenConfig(const char* filename, ScreenLayout& layout);
void drawScreenFromLayout(const ScreenLayout& layout);
void drawElement(const ScreenElement& elem);
float getDataValue(const char* dataSource);
String getDataString(const char* dataSource);
uint16_t parseColor(const char* hexColor);
void initDefaultLayouts();
// ========== END PHASE 2 PROTOTYPES ==========

void IRAM_ATTR tachISR() {
  tachCounter++;
}

// ========== PHASE 2: JSON PARSING & DRAWING FUNCTIONS ==========

// Convert hex color string to uint16_t RGB565
uint16_t parseColor(const char* hexColor) {
    if (hexColor == nullptr || strlen(hexColor) < 4) {
        return 0x0000; // Default to black
    }
    
    // Skip '#' if present
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;
    
    // Parse hex string
    uint32_t color = strtoul(hex, nullptr, 16);
    
    // Convert to RGB565
    if (strlen(hex) == 4) {
        // Short form: RGB -> RRGGBB
        uint8_t r = ((color >> 8) & 0xF) * 17;
        uint8_t g = ((color >> 4) & 0xF) * 17;
        uint8_t b = (color & 0xF) * 17;
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    } else {
        // Full form: RRGGBB
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}

// Parse element type from string
ElementType parseElementType(const char* typeStr) {
    if (strcmp(typeStr, "rect") == 0) return ELEM_RECT;
    if (strcmp(typeStr, "line") == 0) return ELEM_LINE;
    if (strcmp(typeStr, "text") == 0) return ELEM_TEXT_STATIC;
    if (strcmp(typeStr, "dynamic") == 0) return ELEM_TEXT_DYNAMIC;
    if (strcmp(typeStr, "temp") == 0) return ELEM_TEMP_VALUE;
    if (strcmp(typeStr, "coord") == 0) return ELEM_COORD_VALUE;
    if (strcmp(typeStr, "status") == 0) return ELEM_STATUS_VALUE;
    if (strcmp(typeStr, "progress") == 0) return ELEM_PROGRESS_BAR;
    if (strcmp(typeStr, "graph") == 0) return ELEM_GRAPH;
    return ELEM_NONE;
}

// Parse text alignment from string
TextAlign parseAlignment(const char* alignStr) {
    if (strcmp(alignStr, "center") == 0) return ALIGN_CENTER;
    if (strcmp(alignStr, "right") == 0) return ALIGN_RIGHT;
    return ALIGN_LEFT;
}

// Load screen configuration from JSON file
bool loadScreenConfig(const char* filename, ScreenLayout& layout) {
    if (!sdCardAvailable) {
        Serial.printf("[JSON] SD card not available, cannot load %s\n", filename);
        return false;
    }
    
    Serial.printf("[JSON] Loading screen config: %s\n", filename);
    
    // Open file
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("[JSON] Failed to open %s\n", filename);
        return false;
    }
    
    // Read file content
    size_t fileSize = file.size();
    if (fileSize > 8192) {
        Serial.printf("[JSON] File too large: %d bytes (max 8192)\n", fileSize);
        file.close();
        return false;
    }
    
    // Allocate buffer
    char* jsonBuffer = (char*)malloc(fileSize + 1);
    if (!jsonBuffer) {
        Serial.println("[JSON] Failed to allocate memory");
        file.close();
        return false;
    }
    
    // Read file
    size_t bytesRead = file.readBytes(jsonBuffer, fileSize);
    jsonBuffer[bytesRead] = '\0';
    file.close();
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonBuffer);
    free(jsonBuffer);
    
    if (error) {
        Serial.printf("[JSON] Parse error: %s\n", error.c_str());
        return false;
    }
    
    // Extract layout info
    strncpy(layout.name, doc["name"] | "Unnamed", sizeof(layout.name) - 1);
    layout.backgroundColor = parseColor(doc["background"] | "0000");
    layout.elementCount = 0;
    layout.isValid = false;
    
    // Parse elements array
    JsonArray elements = doc["elements"].as<JsonArray>();
    if (!elements) {
        Serial.println("[JSON] No elements array found");
        return false;
    }
    
    int elementIndex = 0;
    for (JsonObject elem : elements) {
        if (elementIndex >= 60) {
            Serial.println("[JSON] Warning: Max 60 elements, ignoring rest");
            break;
        }
        
        ScreenElement& se = layout.elements[elementIndex];
        
        // Parse element properties
        se.type = parseElementType(elem["type"] | "none");
        se.x = elem["x"] | 0;
        se.y = elem["y"] | 0;
        se.w = elem["w"] | 0;
        se.h = elem["h"] | 0;
        se.color = parseColor(elem["color"] | "FFFF");
        se.bgColor = parseColor(elem["bgColor"] | "0000");
        se.textSize = elem["size"] | 2;
        se.decimals = elem["decimals"] | 2;
        se.filled = elem["filled"] | true;
        se.showLabel = elem["showLabel"] | true;
        se.align = parseAlignment(elem["align"] | "left");
        
        // Copy strings
        strncpy(se.label, elem["label"] | "", sizeof(se.label) - 1);
        strncpy(se.dataSource, elem["data"] | "", sizeof(se.dataSource) - 1);
        
        elementIndex++;
    }
    
    layout.elementCount = elementIndex;
    layout.isValid = true;
    
    Serial.printf("[JSON] Loaded %d elements from %s\n", elementIndex, layout.name);
    return true;
}

// Get numeric data value from data source identifier
float getDataValue(const char* dataSource) {
    if (strcmp(dataSource, "posX") == 0) return posX;
    if (strcmp(dataSource, "posY") == 0) return posY;
    if (strcmp(dataSource, "posZ") == 0) return posZ;
    if (strcmp(dataSource, "posA") == 0) return posA;
    
    if (strcmp(dataSource, "wposX") == 0) return wposX;
    if (strcmp(dataSource, "wposY") == 0) return wposY;
    if (strcmp(dataSource, "wposZ") == 0) return wposZ;
    if (strcmp(dataSource, "wposA") == 0) return wposA;
    
    if (strcmp(dataSource, "feedRate") == 0) return feedRate;
    if (strcmp(dataSource, "spindleRPM") == 0) return spindleRPM;
    if (strcmp(dataSource, "psuVoltage") == 0) return psuVoltage;
    if (strcmp(dataSource, "fanSpeed") == 0) return fanSpeed;
    
    if (strcmp(dataSource, "temp0") == 0) return temperatures[0];
    if (strcmp(dataSource, "temp1") == 0) return temperatures[1];
    if (strcmp(dataSource, "temp2") == 0) return temperatures[2];
    if (strcmp(dataSource, "temp3") == 0) return temperatures[3];
    
    return 0.0f;
}

// Get string data value from data source identifier
String getDataString(const char* dataSource) {
    if (strcmp(dataSource, "machineState") == 0) return machineState;
    if (strcmp(dataSource, "ipAddress") == 0) return WiFi.localIP().toString();
    if (strcmp(dataSource, "ssid") == 0) return WiFi.SSID();
    if (strcmp(dataSource, "deviceName") == 0) return String(cfg.device_name);
    if (strcmp(dataSource, "fluidncIP") == 0) return String(cfg.fluidnc_ip);
    
    // Numeric values as strings
    float value = getDataValue(dataSource);
    return String(value, 2);
}
// ========== END PHASE 2 PARSING FUNCTIONS ==========

// ========== PHASE 2: DRAWING FUNCTIONS ==========

// Draw a single screen element
void drawElement(const ScreenElement& elem) {
    switch(elem.type) {
        case ELEM_RECT:
            if (elem.filled) {
                gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.color);
            } else {
                gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);
            }
            break;
            
        case ELEM_LINE:
            if (elem.w > elem.h) {
                // Horizontal line
                gfx.drawFastHLine(elem.x, elem.y, elem.w, elem.color);
            } else {
                // Vertical line
                gfx.drawFastVLine(elem.x, elem.y, elem.h, elem.color);
            }
            break;
            
        case ELEM_TEXT_STATIC:
            gfx.setTextSize(elem.textSize);
            gfx.setTextColor(elem.color);
            gfx.setCursor(elem.x, elem.y);
            gfx.print(elem.label);
            break;
            
        case ELEM_TEXT_DYNAMIC:
            {
                gfx.setTextSize(elem.textSize);
                gfx.setTextColor(elem.color);
                gfx.setCursor(elem.x, elem.y);
                
                String value = getDataString(elem.dataSource);
                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                gfx.print(value);
            }
            break;
            
        case ELEM_TEMP_VALUE:
            {
                gfx.setTextSize(elem.textSize);
                gfx.setTextColor(elem.color);
                gfx.setCursor(elem.x, elem.y);
                
                float temp = getDataValue(elem.dataSource);
                if (cfg.use_fahrenheit) {
                    temp = temp * 9.0 / 5.0 + 32.0;
                }
                
                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                gfx.printf("%.*f%c", elem.decimals, temp, 
                          cfg.use_fahrenheit ? 'F' : 'C');
            }
            break;
            
        case ELEM_COORD_VALUE:
            {
                gfx.setTextSize(elem.textSize);
                gfx.setTextColor(elem.color);
                gfx.setCursor(elem.x, elem.y);
                
                float value = getDataValue(elem.dataSource);
                if (cfg.use_inches) {
                    value = value / 25.4;
                }
                
                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                gfx.printf("%.*f", elem.decimals, value);
            }
            break;
            
        case ELEM_STATUS_VALUE:
            {
                gfx.setTextSize(elem.textSize);
                
                // Color-code machine state
                if (strcmp(elem.dataSource, "machineState") == 0) {
                    if (machineState == "RUN") {
                        gfx.setTextColor(COLOR_GOOD);
                    } else if (machineState == "ALARM") {
                        gfx.setTextColor(COLOR_WARN);
                    } else {
                        gfx.setTextColor(elem.color);
                    }
                } else {
                    gfx.setTextColor(elem.color);
                }
                
                gfx.setCursor(elem.x, elem.y);
                
                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                
                String value = getDataString(elem.dataSource);
                gfx.print(value);
            }
            break;
            
        case ELEM_PROGRESS_BAR:
            {
                // Draw outline
                gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);
                
                // Calculate progress (placeholder - would need job tracking)
                int progress = 0;  // 0-100%
                int fillWidth = (elem.w - 2) * progress / 100;
                
                // Draw filled portion
                if (fillWidth > 0) {
                    gfx.fillRect(elem.x + 1, elem.y + 1, 
                               fillWidth, elem.h - 2, elem.color);
                }
            }
            break;
            
        case ELEM_GRAPH:
            // Placeholder for mini-graph rendering
            gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);
            gfx.setTextSize(1);
            gfx.setTextColor(elem.color);
            gfx.setCursor(elem.x + 5, elem.y + 5);
            gfx.print("GRAPH");
            break;
            
        default:
            break;
    }
}

// Draw entire screen from layout definition
void drawScreenFromLayout(const ScreenLayout& layout) {
    if (!layout.isValid) {
        Serial.println("[JSON] Invalid layout, cannot draw");
        return;
    }
    
    // Clear screen with background color
    gfx.fillScreen(layout.backgroundColor);
    
    // Draw all elements
    for (uint8_t i = 0; i < layout.elementCount; i++) {
        drawElement(layout.elements[i]);
    }
}

// Update only dynamic elements (for efficient screen updates)
void updateDynamicElements(const ScreenLayout& layout) {
    if (!layout.isValid) return;
    
    for (uint8_t i = 0; i < layout.elementCount; i++) {
        const ScreenElement& elem = layout.elements[i];
        
        // Only update elements that display dynamic data
        if (elem.type == ELEM_TEXT_DYNAMIC || 
            elem.type == ELEM_TEMP_VALUE || 
            elem.type == ELEM_COORD_VALUE || 
            elem.type == ELEM_STATUS_VALUE ||
            elem.type == ELEM_PROGRESS_BAR) {
            
            // Clear the element area
            if (elem.w > 0 && elem.h > 0) {
                gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.bgColor);
            }
            
            // Redraw the element
            drawElement(elem);
        }
    }
}

// Initialize default/fallback layouts in case JSON files are missing
void initDefaultLayouts() {
    // Mark all layouts as invalid initially
    monitorLayout.isValid = false;
    alignmentLayout.isValid = false;
    graphLayout.isValid = false;
    networkLayout.isValid = false;
    
    strcpy(monitorLayout.name, "Monitor (Fallback)");
    strcpy(alignmentLayout.name, "Alignment (Fallback)");
    strcpy(graphLayout.name, "Graph (Fallback)");
    strcpy(networkLayout.name, "Network (Fallback)");
    
    Serial.println("[JSON] Default layouts initialized (fallback mode)");
}
// ========== END PHASE 2 DRAWING FUNCTIONS ==========

void setup() {
  Serial.begin(115200);
  Serial.println("FluidDash - Starting...");

  // Initialize default configuration
  initDefaultConfig();

  // Enable watchdog timer (10 seconds)
  enableLoopWDT();
  Serial.println("Watchdog timer enabled (10s timeout)");

  // Initialize display (feed watchdog before long operation)
  feedLoopWDT();
  Serial.println("Initializing display...");
  gfx.init();
  gfx.setRotation(1);  // 90° rotation for landscape mode (480x320)
  gfx.setBrightness(255);
  Serial.println("Display initialized OK");
  gfx.fillScreen(COLOR_BG);
  showSplashScreen();
  delay(2000);  // Show splash briefly

  // Initialize hardware BEFORE drawing (RTC needed for datetime display)
  feedLoopWDT();
  Wire.begin(RTC_SDA, RTC_SCL);  // CYD I2C pins: GPIO32=SDA, GPIO25=SCL

  // Check if RTC is present (may not be connected on CYD)
  if (!rtc.begin()) {
    Serial.println("RTC not found - time display will show 'No RTC'");
    rtcAvailable = false;
  } else {
    Serial.println("RTC initialized");
    rtcAvailable = true;
  }

  pinMode(BTN_MODE, INPUT_PULLUP);

  // RGB LED setup (common anode - LOW=on)
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);    // OFF
  digitalWrite(LED_GREEN, HIGH);  // OFF
  digitalWrite(LED_BLUE, HIGH);   // OFF

  // Configure ADC & PWM
  analogSetWidth(12);
  analogSetAttenuation(ADC_11db);
  ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);  // channel 0
  ledcAttachPin(FAN_PWM, 0);               // attach pin to channel 0
  ledcWrite(0, 0);
  pinMode(FAN_TACH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACH), tachISR, FALLING);

  // Load configuration (overwrites defaults with saved values)
  loadConfig();

  // Allocate history buffer based on config
  allocateHistoryBuffer();

  // Try to connect to saved WiFi credentials
  Serial.println("Attempting WiFi connection...");

  // Read WiFi credentials from preferences
  prefs.begin("fluiddash", true);
  String wifi_ssid = prefs.getString("wifi_ssid", "");
  String wifi_pass = prefs.getString("wifi_pass", "");
  prefs.end();

  if (wifi_ssid.length() > 0) {
    Serial.println("Connecting to: " + wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  } else {
    Serial.println("No saved WiFi credentials");
    WiFi.mode(WIFI_STA);
  }

  feedLoopWDT();

  // Wait up to 10 seconds for connection
  int wifi_retry = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retry < 20) {
    delay(500);
    Serial.print(".");
    wifi_retry++;
    feedLoopWDT();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // Successfully connected to WiFi
    Serial.println("WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    // ========== ADD SD CARD TEST HERE ==========
    feedLoopWDT();
    Serial.println("\n=== Testing SD Card ===");
    
    // Initialize SD card on VSPI bus
    SPIClass spiSD(VSPI);
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    if (SD.begin(SD_CS, spiSD)) {
        sdCardAvailable = true;
        Serial.println("SUCCESS: SD card initialized!");
        
        // Get card info
        uint8_t cardType = SD.cardType();
        Serial.print("SD Card Type: ");
        if (cardType == CARD_MMC) {
            Serial.println("MMC");
        } else if (cardType == CARD_SD) {
            Serial.println("SDSC");
        } else if (cardType == CARD_SDHC) {
            Serial.println("SDHC");
        } else {
            Serial.println("Unknown");
        }
        
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("SD Card Size: %lluMB\n", cardSize);
        
        // Create screens directory for Phase 2
        if (!SD.exists("/screens")) {
            if (SD.mkdir("/screens")) {
                Serial.println("Created directory: /screens");
            } else {
                Serial.println("Failed to create /screens directory");
            }
        } else {
            Serial.println("Directory exists: /screens");
        }
        
    } else {
        sdCardAvailable = false;
        Serial.println("WARNING: SD card not detected");
        Serial.println("Insert SD card and restart for Phase 2 features\n");
    }
    // ========== END SD CARD TEST ==========
    // ========== PHASE 2: LOAD JSON SCREEN LAYOUTS ==========
    feedLoopWDT();
    initDefaultLayouts();  // Initialize fallback state

    if (sdCardAvailable) {
        Serial.println("\n=== Loading JSON Screen Layouts ===");
        
        // Try to load monitor layout
        if (loadScreenConfig("/screens/monitor.json", monitorLayout)) {
            Serial.println("[JSON] Monitor layout loaded successfully");
        } else {
            Serial.println("[JSON] Monitor layout not found or invalid, using fallback");
        }
        
        // Try to load alignment layout (optional for now)
        if (loadScreenConfig("/screens/alignment.json", alignmentLayout)) {
            Serial.println("[JSON] Alignment layout loaded successfully");
        }
        
        // Try to load graph layout (optional for now)
        if (loadScreenConfig("/screens/graph.json", graphLayout)) {
            Serial.println("[JSON] Graph layout loaded successfully");
        }
        
        // Try to load network layout (optional for now)
        if (loadScreenConfig("/screens/network.json", networkLayout)) {
            Serial.println("[JSON] Network layout loaded successfully");
        }
        
        layoutsLoaded = true;
        Serial.println("=== JSON Layout Loading Complete ===\n");
    } else {
        Serial.println("[JSON] SD card not available, using legacy drawing\n");
    }
    // ========== END PHASE 2 LAYOUT LOADING ==========
    feedLoopWDT();

    // Set up mDNS
    if (MDNS.begin(cfg.device_name)) {
      Serial.printf("mDNS started: http://%s.local\n", cfg.device_name);
      MDNS.addService("http", "tcp", 80);
    }

    feedLoopWDT();

    // Connect to FluidNC
    if (cfg.fluidnc_auto_discover) {
      discoverFluidNC();
    } else {
      connectFluidNC();
    }
    feedLoopWDT();
  } else {
    // WiFi connection failed - continue in standalone mode
    Serial.println("WiFi connection failed - continuing in standalone mode");
    Serial.println("Hold button for 10 seconds to enter WiFi configuration mode");

    feedLoopWDT();
  }

  feedLoopWDT();

  // Start web server (always available in STA, AP, or standalone mode)
  Serial.println("Starting web server...");
  setupWebServer();
  feedLoopWDT();

  sessionStartTime = millis();
  currentMode = cfg.default_mode;

  feedLoopWDT();
  delay(2000);
  feedLoopWDT();

  // Clear splash screen and draw the main interface
  Serial.println("Drawing main interface...");
  drawScreen();
  feedLoopWDT();

  Serial.println("Setup complete - entering main loop");
  feedLoopWDT();
}

void loop() {
  // Feed the watchdog timer at the start of each loop iteration
  feedLoopWDT();

  handleButton();

  // Non-blocking ADC sampling (takes one sample every 5ms)
  sampleSensorsNonBlocking();

  // Process complete ADC readings when ready
  if (adcReady) {
    processAdcReadings();
    controlFan();
    adcReady = false;
  }

  if (millis() - lastTachRead >= 1000) {
    calculateRPM();
    lastTachRead = millis();
  }

  if (millis() - lastHistoryUpdate >= (cfg.graph_update_interval * 1000)) {
    updateTempHistory();
    lastHistoryUpdate = millis();
  }

  if (WiFi.status() == WL_CONNECTED) {
      webSocket.loop();
      
      // Always poll for status - FluidNC doesn't have automatic reporting
      if (fluidncConnected && (millis() - lastStatusRequest >= cfg.status_update_rate)) {
          if (debugWebSocket) {
              Serial.println("[FluidNC] Sending status request");
          }
          webSocket.sendTXT("?");
          lastStatusRequest = millis();
      }
      
      // Periodic debug output (only every 10 seconds now)
      static unsigned long lastDebug = 0;
      if (debugWebSocket && millis() - lastDebug >= 10000) {
          Serial.printf("[DEBUG] State:%s MPos:(%.2f,%.2f,%.2f,%.2f) WPos:(%.2f,%.2f,%.2f,%.2f)\n", 
                        machineState.c_str(), 
                        posX, posY, posZ, posA,
                        wposX, wposY, wposZ, wposA);
          lastDebug = millis();
      }
  }


  if (millis() - lastDisplayUpdate >= 1000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Short yield instead of delay for better responsiveness
  yield();
}

// ========== Configuration Management ==========

void loadConfig() {
  prefs.begin("fluiddash", true);
  
  strlcpy(cfg.device_name, prefs.getString("dev_name", "fluiddash").c_str(), 32);
  strlcpy(cfg.fluidnc_ip, prefs.getString("fnc_ip", "192.168.73.13").c_str(), 16);
  cfg.fluidnc_port = prefs.getUShort("fnc_port", 81);  // FluidNC WebSocket default port
  cfg.fluidnc_auto_discover = prefs.getBool("fnc_auto", true);
  
  cfg.temp_threshold_low = prefs.getFloat("temp_low", 30.0);
  cfg.temp_threshold_high = prefs.getFloat("temp_high", 50.0);
  cfg.temp_offset_x = prefs.getFloat("cal_x", 0.0);
  cfg.temp_offset_yl = prefs.getFloat("cal_yl", 0.0);
  cfg.temp_offset_yr = prefs.getFloat("cal_yr", 0.0);
  cfg.temp_offset_z = prefs.getFloat("cal_z", 0.0);
  
  cfg.fan_min_speed = prefs.getUChar("fan_min", 30);
  cfg.fan_max_speed_limit = prefs.getUChar("fan_max", 100);
  
  cfg.psu_voltage_cal = prefs.getFloat("psu_cal", 7.3);
  cfg.psu_alert_low = prefs.getFloat("psu_low", 22.0);
  cfg.psu_alert_high = prefs.getFloat("psu_high", 26.0);
  
  cfg.brightness = prefs.getUChar("bright", 255);
  cfg.default_mode = (DisplayMode)prefs.getUChar("def_mode", 0);
  cfg.show_machine_coords = prefs.getBool("show_mpos", true);
  cfg.show_temp_graph = prefs.getBool("show_graph", true);
  cfg.coord_decimal_places = prefs.getUChar("coord_dec", 2);
  
  cfg.graph_timespan_seconds = prefs.getUShort("graph_time", 300);
  cfg.graph_update_interval = prefs.getUShort("graph_int", 5);
  
  cfg.use_fahrenheit = prefs.getBool("use_f", false);
  cfg.use_inches = prefs.getBool("use_in", false);
  
  cfg.enable_logging = prefs.getBool("logging", false);
  cfg.status_update_rate = prefs.getUShort("status_rate", 200);
  
  prefs.end();
  
  Serial.println("Configuration loaded");
}

void saveConfig() {
  prefs.begin("fluiddash", false);
  
  prefs.putString("dev_name", cfg.device_name);
  prefs.putString("fnc_ip", cfg.fluidnc_ip);
  prefs.putUShort("fnc_port", cfg.fluidnc_port);
  prefs.putBool("fnc_auto", cfg.fluidnc_auto_discover);
  
  prefs.putFloat("temp_low", cfg.temp_threshold_low);
  prefs.putFloat("temp_high", cfg.temp_threshold_high);
  prefs.putFloat("cal_x", cfg.temp_offset_x);
  prefs.putFloat("cal_yl", cfg.temp_offset_yl);
  prefs.putFloat("cal_yr", cfg.temp_offset_yr);
  prefs.putFloat("cal_z", cfg.temp_offset_z);
  
  prefs.putUChar("fan_min", cfg.fan_min_speed);
  prefs.putUChar("fan_max", cfg.fan_max_speed_limit);
  
  prefs.putFloat("psu_cal", cfg.psu_voltage_cal);
  prefs.putFloat("psu_low", cfg.psu_alert_low);
  prefs.putFloat("psu_high", cfg.psu_alert_high);
  
  prefs.putUChar("bright", cfg.brightness);
  prefs.putUChar("def_mode", cfg.default_mode);
  prefs.putBool("show_mpos", cfg.show_machine_coords);
  prefs.putBool("show_graph", cfg.show_temp_graph);
  prefs.putUChar("coord_dec", cfg.coord_decimal_places);
  
  prefs.putUShort("graph_time", cfg.graph_timespan_seconds);
  prefs.putUShort("graph_int", cfg.graph_update_interval);
  
  prefs.putBool("use_f", cfg.use_fahrenheit);
  prefs.putBool("use_in", cfg.use_inches);
  
  prefs.putBool("logging", cfg.enable_logging);
  prefs.putUShort("status_rate", cfg.status_update_rate);
  
  prefs.end();
  
  Serial.println("Configuration saved");
}

void allocateHistoryBuffer() {
  // Calculate required buffer size with safety limit
  historySize = cfg.graph_timespan_seconds / cfg.graph_update_interval;

  // Limit max buffer size to prevent excessive memory usage (max 2000 points = 8KB)
  const uint16_t MAX_BUFFER_SIZE = 2000;
  if (historySize > MAX_BUFFER_SIZE) {
    Serial.printf("Warning: Buffer size %d exceeds limit, capping at %d\n", historySize, MAX_BUFFER_SIZE);
    historySize = MAX_BUFFER_SIZE;
  }

  // Reallocate if needed
  if (tempHistory != nullptr) {
    free(tempHistory);
    tempHistory = nullptr;
  }

  tempHistory = (float*)malloc(historySize * sizeof(float));

  // Check if allocation succeeded
  if (tempHistory == nullptr) {
    Serial.println("ERROR: Failed to allocate history buffer! Restarting...");
    delay(2000);
    ESP.restart();
  }

  // Initialize
  for (int i = 0; i < historySize; i++) {
    tempHistory[i] = 20.0;
  }

  historyIndex = 0;

  Serial.printf("History buffer: %d points (%d seconds, %d bytes)\n",
                historySize, cfg.graph_timespan_seconds, historySize * sizeof(float));
}

// ========== WiFiManager Setup ==========

void setupWiFiManager() {
  // Custom parameters for captive portal
  WiFiManagerParameter custom_fluidnc_ip("fluidnc_ip", "FluidNC IP Address", cfg.fluidnc_ip, 16);
  WiFiManagerParameter custom_device_name("dev_name", "Device Name", cfg.device_name, 32);
  
  // Add parameters to WiFiManager
  wm.addParameter(&custom_fluidnc_ip);
  wm.addParameter(&custom_device_name);
  
  // Callbacks to save custom parameters
  wm.setSaveConfigCallback([]() {
    Serial.println("Saving custom parameters...");
  });
  
  // Set timeout for captive portal
  wm.setConfigPortalTimeout(180); // 3 minutes
}

// ========== Web Server Setup ==========

void setupWebServer() {
  // Serve main configuration page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getMainHTML());
  });
  
  // User settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getSettingsHTML());
  });
  
  // Admin/calibration page
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getAdminHTML());
  });

  // WiFi configuration page
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getWiFiConfigHTML());
  });

  // API: Get current config as JSON
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getConfigJSON());
  });
  
  // API: Get current status as JSON
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getStatusJSON());
  });
  
  // API: Save settings
  server.on("/api/save", HTTP_POST, [](AsyncWebServerRequest *request){
    // Update config from POST parameters
    if (request->hasParam("temp_low", true)) {
      cfg.temp_threshold_low = request->getParam("temp_low", true)->value().toFloat();
    }
    if (request->hasParam("temp_high", true)) {
      cfg.temp_threshold_high = request->getParam("temp_high", true)->value().toFloat();
    }
    if (request->hasParam("fan_min", true)) {
      cfg.fan_min_speed = request->getParam("fan_min", true)->value().toInt();
    }
    if (request->hasParam("graph_time", true)) {
      uint16_t newTime = request->getParam("graph_time", true)->value().toInt();
      if (newTime != cfg.graph_timespan_seconds) {
        cfg.graph_timespan_seconds = newTime;
        allocateHistoryBuffer(); // Reallocate with new size
      }
    }
    if (request->hasParam("graph_interval", true)) {
      cfg.graph_update_interval = request->getParam("graph_interval", true)->value().toInt();
    }
    if (request->hasParam("psu_low", true)) {
      cfg.psu_alert_low = request->getParam("psu_low", true)->value().toFloat();
    }
    if (request->hasParam("psu_high", true)) {
      cfg.psu_alert_high = request->getParam("psu_high", true)->value().toFloat();
    }
    if (request->hasParam("coord_decimals", true)) {
      cfg.coord_decimal_places = request->getParam("coord_decimals", true)->value().toInt();
    }
    
    saveConfig();
    request->send(200, "text/plain", "Settings saved successfully");
  });
  
  // API: Save admin/calibration settings
  server.on("/api/admin/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("cal_x", true)) {
      cfg.temp_offset_x = request->getParam("cal_x", true)->value().toFloat();
    }
    if (request->hasParam("cal_yl", true)) {
      cfg.temp_offset_yl = request->getParam("cal_yl", true)->value().toFloat();
    }
    if (request->hasParam("cal_yr", true)) {
      cfg.temp_offset_yr = request->getParam("cal_yr", true)->value().toFloat();
    }
    if (request->hasParam("cal_z", true)) {
      cfg.temp_offset_z = request->getParam("cal_z", true)->value().toFloat();
    }
    if (request->hasParam("psu_cal", true)) {
      cfg.psu_voltage_cal = request->getParam("psu_cal", true)->value().toFloat();
    }
    
    saveConfig();
    request->send(200, "text/plain", "Calibration saved successfully");
  });
  
  // Reset WiFi settings
  server.on("/api/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Resetting WiFi - device will restart");
    delay(1000);
    wm.resetSettings();
    ESP.restart();
  });
  
  // Restart device
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Restarting...");
    delay(1000);
    ESP.restart();
  });

  // WiFi scanning removed - ESP32 cannot scan while in AP mode
  // Users must manually enter SSID and password

  // API: Connect to WiFi network
  server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = "";
    String password = "";

    if (request->hasParam("ssid", true)) {
      ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
    }

    if (ssid.length() == 0) {
      request->send(200, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
      return;
    }

    Serial.println("Attempting to connect to: " + ssid);

    // Store credentials in preferences
    prefs.begin("fluiddash", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", password);
    prefs.end();

    // Send response and restart to apply credentials
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved. Device will restart and attempt to connect.\"}");

    Serial.println("WiFi credentials saved. Restarting...");
    delay(2000);
    ESP.restart();
  });

  // API: Reload screen layouts from SD card
  server.on("/api/reload-screens", HTTP_POST, [](AsyncWebServerRequest *request){
      if (!sdCardAvailable) {
          request->send(200, "application/json", "{\"success\":false,\"message\":\"SD card not available\"}");
          return;
      }
      
      Serial.println("[JSON] Reloading screen layouts...");
      
      int loaded = 0;
      if (loadScreenConfig("/screens/monitor.json", monitorLayout)) loaded++;
      if (loadScreenConfig("/screens/alignment.json", alignmentLayout)) loaded++;
      if (loadScreenConfig("/screens/graph.json", graphLayout)) loaded++;
      if (loadScreenConfig("/screens/network.json", networkLayout)) loaded++;
      
      // Redraw current screen
      drawScreen();
      
      char response[128];
      sprintf(response, "{\"success\":true,\"message\":\"Reloaded %d layouts\"}", loaded);
      request->send(200, "application/json", response);
      
      Serial.printf("[JSON] Reloaded %d layouts\n", loaded);
  });
    // ========== WEB JSON UPLOAD & EDITOR ==========
  
  // Upload page
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){
      String html = "<!DOCTYPE html><html><head><title>Upload JSON</title>";
      html += "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff}";
      html += "h1{color:#00bfff}.box{background:#2a2a2a;padding:20px;border-radius:8px;max-width:600px}";
      html += "button{background:#00bfff;color:#000;padding:10px 20px;border:none;cursor:pointer}";
      html += "#status{margin-top:20px;padding:10px}.success{background:#004d00;color:#0f0}";
      html += ".error{background:#4d0000;color:#f00}</style></head><body>";
      html += "<h1>Upload JSON</h1><div class='box'><h3>Upload Screen Layout</h3>";
      html += "<form id='f' enctype='multipart/form-data'>";
      html += "<input type='file' id='file' accept='.json' required><br><br>";
      html += "<button type='submit'>Upload</button></form>";
      html += "<div id='status'></div></div>";
      html += "<script>document.getElementById('f').addEventListener('submit',function(e){";
      html += "e.preventDefault();let file=document.getElementById('file').files[0];";
      html += "if(!file)return;let s=document.getElementById('status');";
      html += "s.innerHTML='Uploading...';s.className='';let fd=new FormData();";
      html += "fd.append('file',file);fetch('/upload-json',{method:'POST',body:fd})";
      html += ".then(r=>r.json()).then(d=>{if(d.success){s.innerHTML='Uploaded!';";
      html += "s.className='success';fetch('/api/reload-screens',{method:'POST'})}";
      html += "else{s.innerHTML='Error';s.className='error'}})";
      html += ".catch(e=>{s.innerHTML='Failed';s.className='error'})});</script>";
      html += "</body></html>";
      request->send(200, "text/html", html);
  });
  
  // Handle file upload
  server.on("/upload-json", HTTP_POST, 
      [](AsyncWebServerRequest *request){ request->send(200); },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
          static File uploadFile;
          if(index == 0){
              if(!filename.endsWith(".json")){
                  Serial.println("[Upload] Not a JSON file");
                  return;
              }
              String path = "/screens/" + filename;
              uploadFile = SD.open(path, FILE_WRITE);
              if(!uploadFile){
                  Serial.printf("[Upload] Cannot create %s\n", path.c_str());
                  return;
              }
          }
          if(uploadFile && len){
              uploadFile.write(data, len);
          }
          if(final && uploadFile){
              uploadFile.close();
              Serial.printf("[Upload] Complete: %s\n", filename.c_str());
          }
      }
  );
  
  // Get JSON file
  server.on("/get-json", HTTP_GET, [](AsyncWebServerRequest *request){
      if(!request->hasParam("file")){
          request->send(400, "application/json", "{\"success\":false}");
          return;
      }
      String filename = request->getParam("file")->value();
      String path = "/screens/" + filename;
      File file = SD.open(path, FILE_READ);
      if(!file){
          request->send(404, "application/json", "{\"success\":false}");
          return;
      }
      String content = file.readString();
      file.close();
      String response = "{\"success\":true,\"content\":" + content + "}";
      request->send(200, "application/json", response);
  });
  
  // Save JSON file
  server.on("/save-json", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
          static String jsonData;
          if(index == 0) jsonData = "";
          for(size_t i = 0; i < len; i++) jsonData += (char)data[i];
          if(index + len == total){
              JsonDocument doc;
              if(deserializeJson(doc, jsonData)){
                  request->send(400, "application/json", "{\"success\":false}");
                  return;
              }
              String filename = doc["filename"];
              String content = doc["content"];
              String path = "/screens/" + filename;
              File file = SD.open(path, FILE_WRITE);
              if(!file){
                  request->send(500, "application/json", "{\"success\":false}");
                  return;
              }
              file.print(content);
              file.close();
              Serial.printf("[Editor] Saved: %s\n", path.c_str());
              request->send(200, "application/json", "{\"success\":true}");
          }
      }
  );
  
  // Editor page
  server.on("/editor", HTTP_GET, [](AsyncWebServerRequest *request){
      String html = "<!DOCTYPE html><html><head><title>JSON Editor</title>";
      html += "<style>body{margin:0;background:#1a1a1a;color:#fff;font-family:monospace}";
      html += ".container{display:flex;height:100vh}.sidebar{width:200px;background:#2a2a2a;padding:10px}";
      html += ".editor{flex:1;display:flex;flex-direction:column;padding:10px}";
      html += "textarea{flex:1;background:#0a0a0a;color:#0f0;border:1px solid #00bfff;padding:10px;font-family:monospace}";
      html += "button{background:#00bfff;color:#000;padding:10px 20px;border:none;margin:5px;cursor:pointer}";
      html += ".file-btn{background:#2a2a2a;color:#fff;padding:8px;margin:2px 0;width:100%;text-align:left}";
      html += "#status{padding:10px;margin:10px 0}.success{background:#004d00;color:#0f0}";
      html += ".error{background:#4d0000;color:#f00}</style></head><body>";
      html += "<div class='container'><div class='sidebar'><h3>Files</h3>";
      html += "<button class='file-btn' onclick=\"loadFile('monitor.json')\">monitor.json</button>";
      html += "<button class='file-btn' onclick=\"loadFile('alignment.json')\">alignment.json</button>";
      html += "<button class='file-btn' onclick=\"loadFile('graph.json')\">graph.json</button>";
      html += "<button class='file-btn' onclick=\"loadFile('network.json')\">network.json</button></div>";
      html += "<div class='editor'><h2 id='filename'>Select file</h2>";
      html += "<textarea id='editor' placeholder='Load a JSON file...'></textarea><div>";
      html += "<button onclick='saveFile()'>Save</button>";
      html += "<button onclick='validate()'>Validate</button>";
      html += "<button onclick='reload()'>Reload</button></div>";
      html += "<div id='status'></div></div></div>";
      html += "<script>let cf='';function show(m,t){document.getElementById('status').innerHTML=m;";
      html += "document.getElementById('status').className=t}";
      html += "function loadFile(f){cf=f;document.getElementById('filename').innerText='Editing: '+f;";
      html += "fetch('/get-json?file='+f).then(r=>r.json()).then(d=>{if(d.success){";
      html += "document.getElementById('editor').value=JSON.stringify(d.content,null,2);";
      html += "show('Loaded','success')}else{show('Not found','error')}})}";
      html += "function saveFile(){if(!cf){show('No file','error');return}";
      html += "let c=document.getElementById('editor').value;try{JSON.parse(c)}catch(e){";
      html += "show('Invalid JSON','error');return}";
      html += "fetch('/save-json',{method:'POST',headers:{'Content-Type':'application/json'},";
      html += "body:JSON.stringify({filename:cf,content:c})}).then(r=>r.json()).then(d=>{";
      html += "if(d.success)show('Saved!','success');else show('Failed','error')})}";
      html += "function validate(){try{let c=document.getElementById('editor').value;";
      html += "let p=JSON.parse(c);show('Valid ('+p.elements.length+' elements)','success')}";
      html += "catch(e){show('Invalid JSON','error')}}";
      html += "function reload(){fetch('/api/reload-screens',{method:'POST'}).then(r=>r.json()).then(d=>{";
      html += "show('Reloaded!','success')})}</script></body></html>";
      request->send(200, "text/html", html);
  });
  
  // ========== END WEB JSON UPLOAD & EDITOR ==========

  server.begin();
  Serial.println("Web server started");
}

// ========== HTML Pages ==========
// NOTE: These functions return large String objects which can cause heap fragmentation.
// For production use, consider using AsyncWebServerResponse streaming or PROGMEM storage.
// Current implementation is acceptable for occasional web interface access.

String getMainHTML() {
  // Reserve memory upfront to reduce fragmentation
  String html;
  html.reserve(4096);
  html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 800px; margin: 0 auto; }
    h1 { color: #00bfff; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    .status-item { padding: 10px; background: #333; border-radius: 5px; }
    .status-label { color: #888; font-size: 12px; }
    .status-value { font-size: 24px; font-weight: bold; color: #00bfff; }
    .temp-ok { color: #00ff00; }
    .temp-warn { color: #ffaa00; }
    .temp-hot { color: #ff0000; }
    button { background: #00bfff; color: #fff; border: none; padding: 12px 24px; 
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 5px; }
    button:hover { background: #0099cc; }
    .link-button { display: inline-block; text-decoration: none; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>🛡️ FluidDash</h1>
    
    <div class='card'>
      <h2>System Status</h2>
      <div class='status-grid' id='status'>
        <div class='status-item'>
          <div class='status-label'>CNC Status</div>
          <div class='status-value' id='cnc_status'>Loading...</div>
        </div>
        <div class='status-item'>
          <div class='status-label'>Max Temperature</div>
          <div class='status-value' id='max_temp'>--°C</div>
        </div>
        <div class='status-item'>
          <div class='status-label'>Fan Speed</div>
          <div class='status-value' id='fan_speed'>--%</div>
        </div>
        <div class='status-item'>
          <div class='status-label'>PSU Voltage</div>
          <div class='status-value' id='psu_volt'>--V</div>
        </div>
      </div>
    </div>
    
    <div class='card'>
      <h2>Configuration</h2>
      <a href='/settings' class='link-button'><button>⚙️ User Settings</button></a>
      <a href='/admin' class='link-button'><button>🔧 Admin/Calibration</button></a>
      <a href='/wifi' class='link-button'><button>📡 WiFi Setup</button></a>
      <button onclick='restart()'>🔄 Restart Device</button>
    </div>
    
    <div class='card'>
      <h2>Information</h2>
      <p><strong>Device Name:</strong> )";
  html += cfg.device_name;
  html += R"(.local</p>
      <p><strong>IP Address:</strong> )";
  html += WiFi.localIP().toString();
  html += R"(</p>
      <p><strong>FluidNC:</strong> )";
  html += cfg.fluidnc_ip;
  html += R"(</p>
      <p><strong>Version:</strong> v0.7</p>
    </div>
  </div>
  
  <script>
    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('cnc_status').textContent = data.machine_state;
          
          let maxTemp = Math.max(...data.temperatures);
          let tempEl = document.getElementById('max_temp');
          tempEl.textContent = maxTemp.toFixed(1) + '°C';
          tempEl.className = 'status-value ' + 
            (maxTemp > 50 ? 'temp-hot' : maxTemp > 35 ? 'temp-warn' : 'temp-ok');
          
          document.getElementById('fan_speed').textContent = data.fan_speed + '%';
          document.getElementById('psu_volt').textContent = data.psu_voltage.toFixed(1) + 'V';
        });
    }
    
    function restart() {
      if (confirm('Restart device?')) {
        fetch('/api/restart', {method: 'POST'})
          .then(() => alert('Restarting... Reconnect in 30 seconds'));
      }
    }
    
    function resetWiFi() {
      if (confirm('Reset WiFi settings? Device will restart in AP mode.')) {
        fetch('/api/reset-wifi', {method: 'POST'})
          .then(() => alert('WiFi reset. Connect to FluidDash-Setup network.'));
      }
    }
    
    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
)";
  return html;
}

String getSettingsHTML() {
  String html;
  html.reserve(5120);
  html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Settings - FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 600px; margin: 0 auto; }
    h1, h2 { color: #00bfff; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    label { display: block; margin: 15px 0 5px; color: #aaa; }
    input, select { width: 100%; padding: 10px; background: #333; color: #fff; 
                    border: 1px solid #555; border-radius: 5px; box-sizing: border-box; }
    button { background: #00bfff; color: #fff; border: none; padding: 12px 24px; 
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 5px 0 0; }
    button:hover { background: #0099cc; }
    .back-btn { background: #666; }
    .back-btn:hover { background: #555; }
    .success { background: #00ff00; color: #000; padding: 10px; border-radius: 5px; 
               margin: 10px 0; display: none; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>⚙️ User Settings</h1>
    
    <form id='settingsForm'>
      <div class='card'>
        <h2>Temperature Control</h2>
        <label>Low Threshold (°C) - Fan starts ramping up</label>
        <input type='number' name='temp_low' value=')" + String(cfg.temp_threshold_low) + R"(' step='0.5' min='20' max='50'>
        
        <label>High Threshold (°C) - Fan at 100%</label>
        <input type='number' name='temp_high' value=')" + String(cfg.temp_threshold_high) + R"(' step='0.5' min='30' max='80'>
      </div>
      
      <div class='card'>
        <h2>Fan Control</h2>
        <label>Minimum Fan Speed (%)</label>
        <input type='number' name='fan_min' value=')" + String(cfg.fan_min_speed) + R"(' min='0' max='100'>
      </div>
      
      <div class='card'>
        <h2>PSU Monitoring</h2>
        <label>Low Voltage Alert (V)</label>
        <input type='number' name='psu_low' value=')" + String(cfg.psu_alert_low) + R"(' step='0.1' min='18' max='24'>
        
        <label>High Voltage Alert (V)</label>
        <input type='number' name='psu_high' value=')" + String(cfg.psu_alert_high) + R"(' step='0.1' min='24' max='30'>
      </div>
      
      <div class='card'>
        <h2>Temperature Graph</h2>
        <label>Graph Timespan (seconds)</label>
        <select name='graph_time'>
          <option value='60' )" + String(cfg.graph_timespan_seconds == 60 ? "selected" : "") + R"(>1 minute</option>
          <option value='300' )" + String(cfg.graph_timespan_seconds == 300 ? "selected" : "") + R"(>5 minutes</option>
          <option value='600' )" + String(cfg.graph_timespan_seconds == 600 ? "selected" : "") + R"(>10 minutes</option>
          <option value='1800' )" + String(cfg.graph_timespan_seconds == 1800 ? "selected" : "") + R"(>30 minutes</option>
          <option value='3600' )" + String(cfg.graph_timespan_seconds == 3600 ? "selected" : "") + R"(>60 minutes</option>
        </select>
        
        <label>Update Interval (seconds)</label>
        <select name='graph_interval'>
          <option value='1' )" + String(cfg.graph_update_interval == 1 ? "selected" : "") + R"(>1 second</option>
          <option value='5' )" + String(cfg.graph_update_interval == 5 ? "selected" : "") + R"(>5 seconds</option>
          <option value='10' )" + String(cfg.graph_update_interval == 10 ? "selected" : "") + R"(>10 seconds</option>
          <option value='30' )" + String(cfg.graph_update_interval == 30 ? "selected" : "") + R"(>30 seconds</option>
          <option value='60' )" + String(cfg.graph_update_interval == 60 ? "selected" : "") + R"(>60 seconds</option>
        </select>
      </div>
      
      <div class='card'>
        <h2>Display</h2>
        <label>Coordinate Decimal Places</label>
        <select name='coord_decimals'>
          <option value='2' )" + String(cfg.coord_decimal_places == 2 ? "selected" : "") + R"(>2 decimals (0.00)</option>
          <option value='3' )" + String(cfg.coord_decimal_places == 3 ? "selected" : "") + R"(>3 decimals (0.000)</option>
        </select>
      </div>
      
      <div class='success' id='success'>Settings saved successfully!</div>
      
      <button type='submit'>💾 Save Settings</button>
      <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
    </form>
  </div>
  
  <script>
    document.getElementById('settingsForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      let formData = new FormData(this);
      
      fetch('/api/save', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(r => r.text())
      .then(msg => {
        document.getElementById('success').style.display = 'block';
        setTimeout(() => {
          document.getElementById('success').style.display = 'none';
        }, 3000);
      });
    });
  </script>
</body>
</html>
)";
  return html;
}

String getAdminHTML() {
  String html;
  html.reserve(5120);
  html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Admin - FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 600px; margin: 0 auto; }
    h1, h2 { color: #ff6600; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    .warning { background: #ff6600; color: #000; padding: 15px; border-radius: 5px; 
               margin: 15px 0; font-weight: bold; }
    label { display: block; margin: 15px 0 5px; color: #aaa; }
    input { width: 100%; padding: 10px; background: #333; color: #fff; 
            border: 1px solid #555; border-radius: 5px; box-sizing: border-box; }
    button { background: #ff6600; color: #fff; border: none; padding: 12px 24px; 
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 5px 0 0; }
    button:hover { background: #cc5200; }
    .back-btn { background: #666; }
    .back-btn:hover { background: #555; }
    .success { background: #00ff00; color: #000; padding: 10px; border-radius: 5px; 
               margin: 10px 0; display: none; }
    .current-reading { color: #00bfff; font-size: 18px; margin: 5px 0; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>🔧 Admin & Calibration</h1>
    
    <div class='warning'>
      ⚠️ Warning: These settings affect measurement accuracy. 
      Only change if you have calibration equipment.
    </div>
    
    <div class='card'>
      <h2>Current Readings (Uncalibrated)</h2>
      <div id='readings'>Loading...</div>
    </div>
    
    <form id='adminForm'>
      <div class='card'>
        <h2>Temperature Calibration</h2>
        <p style='color:#aaa'>Enter offset to add/subtract from each sensor</p>
        
        <label>X-Axis Offset (°C)</label>
        <input type='number' name='cal_x' value=')" + String(cfg.temp_offset_x, 2) + R"(' step='0.1'>
        
        <label>YL-Axis Offset (°C)</label>
        <input type='number' name='cal_yl' value=')" + String(cfg.temp_offset_yl, 2) + R"(' step='0.1'>
        
        <label>YR-Axis Offset (°C)</label>
        <input type='number' name='cal_yr' value=')" + String(cfg.temp_offset_yr, 2) + R"(' step='0.1'>
        
        <label>Z-Axis Offset (°C)</label>
        <input type='number' name='cal_z' value=')" + String(cfg.temp_offset_z, 2) + R"(' step='0.1'>
      </div>
      
      <div class='card'>
        <h2>PSU Voltage Calibration</h2>
        <p style='color:#aaa'>Voltage divider multiplier (measure with multimeter)</p>
        
        <label>Calibration Factor</label>
        <input type='number' name='psu_cal' value=')" + String(cfg.psu_voltage_cal, 3) + R"(' step='0.01' min='5' max='10'>
      </div>
      
      <div class='success' id='success'>Calibration saved successfully!</div>
      
      <button type='submit'>💾 Save Calibration</button>
      <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
    </form>
  </div>
  
  <script>
    function updateReadings() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          let html = '';
          ['X', 'YL', 'YR', 'Z'].forEach((name, i) => {
            html += `<div class='current-reading'>${name}: ${data.temperatures[i].toFixed(2)}°C</div>`;
          });
          html += `<div class='current-reading'>PSU: ${data.psu_voltage.toFixed(2)}V</div>`;
          document.getElementById('readings').innerHTML = html;
        });
    }
    
    document.getElementById('adminForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      let formData = new FormData(this);
      
      fetch('/api/admin/save', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(r => r.text())
      .then(msg => {
        document.getElementById('success').style.display = 'block';
        setTimeout(() => {
          document.getElementById('success').style.display = 'none';
        }, 3000);
      });
    });
    
    updateReadings();
    setInterval(updateReadings, 2000);
  </script>
</body>
</html>
)";
  return html;
}

String getWiFiConfigHTML() {
  String html;
  html.reserve(5120);

  // Get current WiFi status
  String currentSSID = WiFi.SSID();
  String currentIP = WiFi.localIP().toString();
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  bool isAPMode = inAPMode;

  html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>WiFi Setup - FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 600px; margin: 0 auto; }
    h1 { color: #00bfff; }
    h2 { color: #00ff00; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    .status { padding: 15px; border-radius: 5px; margin: 15px 0; font-weight: bold; }
    .status-connected { background: #00ff00; color: #000; }
    .status-ap { background: #ff9900; color: #000; }
    .status-disconnected { background: #ff0000; color: #fff; }
    label { display: block; margin: 15px 0 5px; color: #aaa; }
    input, select { width: 100%; padding: 10px; background: #333; color: #fff;
            border: 1px solid #555; border-radius: 5px; box-sizing: border-box;
            font-size: 16px; }
    button { background: #00bfff; color: #fff; border: none; padding: 12px 24px;
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 5px 0 0; }
    button:hover { background: #0099cc; }
    .back-btn { background: #666; }
    .back-btn:hover { background: #555; }
    .message { padding: 10px; border-radius: 5px; margin: 10px 0; display: none; }
    .success { background: #00ff00; color: #000; }
    .error { background: #ff0000; color: #fff; }
    #password { -webkit-text-security: disc; }
    .info-box { background: #1a3a5a; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #00bfff; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>📡 WiFi Configuration</h1>

)";

  // Add current status
  html += "<div class='status ";
  if (isAPMode) {
    html += "status-ap'>🔧 AP Mode Active - Configure WiFi to connect to your network</div>";
  } else if (isConnected) {
    html += "status-connected'>✅ Connected to: " + currentSSID + "<br>IP: " + currentIP + "</div>";
  } else {
    html += "status-disconnected'>❌ Not Connected - Configure WiFi below</div>";
  }

  html += R"(

    <div class='info-box'>
      <strong>ℹ️ Manual WiFi Configuration</strong><br>
      Enter your WiFi network name (SSID) and password below. The device will restart and attempt to connect.
    </div>

    <form id='wifiForm'>
      <div class='card'>
        <h2>WiFi Credentials</h2>

        <label>Network Name (SSID)</label>
        <input type='text' id='ssid' name='ssid' value=')" + currentSSID + R"(' required
               placeholder='Enter WiFi network name'>

        <label>Password</label>
        <input type='password' id='password' name='password' required
               placeholder='Enter WiFi password'>

        <div class='message' id='message'></div>

        <button type='submit'>💾 Save & Connect</button>
        <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
      </div>
    </form>
  </div>

  <script>

    document.getElementById('wifiForm').addEventListener('submit', function(e) {
      e.preventDefault();

      let ssid = document.getElementById('ssid').value;
      let password = document.getElementById('password').value;
      let msgDiv = document.getElementById('message');

      msgDiv.style.display = 'block';
      msgDiv.className = 'message';
      msgDiv.textContent = 'Connecting to ' + ssid + '...';

      let formData = new FormData();
      formData.append('ssid', ssid);
      formData.append('password', password);

      fetch('/api/wifi/connect', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          msgDiv.className = 'message success';
          msgDiv.innerHTML = '✅ Connected successfully!<br>Device will restart in 3 seconds...';
          setTimeout(() => {
            window.location.href = '/';
          }, 3000);
        } else {
          msgDiv.className = 'message error';
          msgDiv.textContent = '❌ Connection failed: ' + data.message;
        }
      })
      .catch(err => {
        msgDiv.className = 'message error';
        msgDiv.textContent = '❌ Request failed. Check connection.';
      });
    });
  </script>
</body>
</html>
)";
  return html;
}

// Continue with JSON APIs and improved display in next section...
// ========== JSON API Functions ==========

String getConfigJSON() {
  String json = "{";
  json += "\"device_name\":\"" + String(cfg.device_name) + "\",";
  json += "\"fluidnc_ip\":\"" + String(cfg.fluidnc_ip) + "\",";
  json += "\"temp_low\":" + String(cfg.temp_threshold_low) + ",";
  json += "\"temp_high\":" + String(cfg.temp_threshold_high) + ",";
  json += "\"fan_min\":" + String(cfg.fan_min_speed) + ",";
  json += "\"psu_low\":" + String(cfg.psu_alert_low) + ",";
  json += "\"psu_high\":" + String(cfg.psu_alert_high) + ",";
  json += "\"graph_time\":" + String(cfg.graph_timespan_seconds) + ",";
  json += "\"graph_interval\":" + String(cfg.graph_update_interval);
  json += "}";
  return json;
}

String getStatusJSON() {
  String json = "{";
  json += "\"machine_state\":\"" + machineState + "\",";
  json += "\"temperatures\":[";
  for (int i = 0; i < 4; i++) {
    json += String(temperatures[i], 2);
    if (i < 3) json += ",";
  }
  json += "],";
  json += "\"fan_speed\":" + String(fanSpeed) + ",";
  json += "\"fan_rpm\":" + String(fanRPM) + ",";
  json += "\"psu_voltage\":" + String(psuVoltage, 2) + ",";
  json += "\"wpos_x\":" + String(wposX, 3) + ",";
  json += "\"wpos_y\":" + String(wposY, 3) + ",";
  json += "\"wpos_z\":" + String(wposZ, 3) + ",";
  json += "\"mpos_x\":" + String(posX, 3) + ",";
  json += "\"mpos_y\":" + String(posY, 3) + ",";
  json += "\"mpos_z\":" + String(posZ, 3) + ",";
  json += "\"connected\":" + String(fluidncConnected ? "true" : "false");
  json += "}";
  return json;
}

// ========== FluidNC Connection ==========

void connectFluidNC() {
    Serial.printf("[FluidNC] Attempting to connect to ws://%s:%d/ws\n", 
                  cfg.fluidnc_ip, cfg.fluidnc_port);
    webSocket.begin(cfg.fluidnc_ip, cfg.fluidnc_port, "/ws");  // Add /ws path
    webSocket.onEvent(fluidNCWebSocketEvent);
    webSocket.setReconnectInterval(5000);
    Serial.println("[FluidNC] WebSocket initialized, waiting for connection...");
}

void discoverFluidNC() {
  Serial.println("Auto-discovering FluidNC...");
  
  // Try mDNS discovery first
  int n = MDNS.queryService("http", "tcp");
  for (int i = 0; i < n; i++) {
    String hostname = MDNS.hostname(i);
    if (hostname.indexOf("fluidnc") >= 0) {
      IPAddress ip = MDNS.IP(i);
      strlcpy(cfg.fluidnc_ip, ip.toString().c_str(), sizeof(cfg.fluidnc_ip));
      Serial.printf("Found FluidNC at: %s\n", cfg.fluidnc_ip);
      connectFluidNC();
      return;
    }
  }
  
  // Fallback to configured IP
  Serial.println("Using configured FluidNC IP");
  connectFluidNC();
}

void fluidNCWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[FluidNC] Disconnected!");
            fluidncConnected = false;
            machineState = "OFFLINE";
            break;
            
        case WStype_CONNECTED:
            Serial.printf("[FluidNC] Connected to: %s\n", payload);
            fluidncConnected = true;
            machineState = "IDLE";
            
            // DON'T send ReportInterval - FluidNC doesn't support it
            // We'll use manual polling with ? status requests
            reportingSetupTime = millis();
            break;        

        case WStype_TEXT:
            {
                char* msg = (char*)payload;
                if (debugWebSocket) {
                    Serial.printf("[FluidNC] RX TEXT (%d bytes): ", length);
                    for(size_t i = 0; i < length; i++) {
                        Serial.print(msg[i]);
                    }
                    Serial.println();
                }
                
                String msgStr = String(msg);
                if (msgStr.startsWith("<")) {
                    parseFluidNCStatus(msgStr);
                } else if (msgStr.startsWith("ALARM:")) {
                    machineState = "ALARM";
                    parseFluidNCStatus(msgStr);
                }
            }
            break;
            
        case WStype_BIN:
            {
                // FluidNC sends status as BINARY data
                if (debugWebSocket) {
                    Serial.printf("[FluidNC] RX BINARY (%d bytes): ", length);
                }
                
                // Convert binary payload to null-terminated string
                char* msg = (char*)malloc(length + 1);
                if (msg != nullptr) {
                    memcpy(msg, payload, length);
                    msg[length] = '\0';
                    
                    if (debugWebSocket) {
                        Serial.println(msg);
                    }
                    
                    // Parse the status message
                    String msgStr = String(msg);
                    parseFluidNCStatus(msgStr);
                    
                    free(msg);
                } else {
                    Serial.println("[FluidNC] ERROR: Failed to allocate memory");
                }
            }
            break;
            
        case WStype_ERROR:
            Serial.println("[FluidNC] WebSocket Error!");
            break;
            
        case WStype_PING:
            // Ping/pong for keep-alive - normal, no logging needed
            break;
            
        case WStype_PONG:
            break;
            
        default:
            if (debugWebSocket) {
                Serial.printf("[FluidNC] Event type: %d\n", type);
            }
            break;
    }
}

void parseFluidNCStatus(String status) {
    String oldState = machineState;
    
    // Parse state (between < and |)
    int stateEnd = status.indexOf('|');
    if (stateEnd > 0) {
        String state = status.substring(1, stateEnd);
        machineState = state;
        machineState.toUpperCase();
        
        // Job tracking
        if (oldState != "RUN" && machineState == "RUN") {
            jobStartTime = millis();
            isJobRunning = true;
        }
        if (oldState == "RUN" && machineState != "RUN") {
            isJobRunning = false;
        }
    }
    
    // Parse MPos (Machine Position) - supports 3 or 4 axes
    int mposIndex = status.indexOf("MPos:");
    if (mposIndex >= 0) {
        int endIndex = status.indexOf('|', mposIndex);
        if (endIndex < 0) endIndex = status.indexOf('>', mposIndex);
        String posStr = status.substring(mposIndex + 5, endIndex);
        
        // Count commas to determine axis count
        int commaCount = 0;
        for (int i = 0; i < posStr.length(); i++) {
            if (posStr.charAt(i) == ',') commaCount++;
        }
        
        if (commaCount >= 3) {
            // 4-axis machine
            sscanf(posStr.c_str(), "%f,%f,%f,%f", &posX, &posY, &posZ, &posA);
        } else {
            // 3-axis machine
            sscanf(posStr.c_str(), "%f,%f,%f", &posX, &posY, &posZ);
            posA = 0;
        }
    }
    
    // Parse WPos (Work Position) if present
    int wposIndex = status.indexOf("WPos:");
    if (wposIndex >= 0) {
        int endIndex = status.indexOf('|', wposIndex);
        if (endIndex < 0) endIndex = status.indexOf('>', wposIndex);
        String posStr = status.substring(wposIndex + 5, endIndex);
        
        int commaCount = 0;
        for (int i = 0; i < posStr.length(); i++) {
            if (posStr.charAt(i) == ',') commaCount++;
        }
        
        if (commaCount >= 3) {
            sscanf(posStr.c_str(), "%f,%f,%f,%f", &wposX, &wposY, &wposZ, &wposA);
        } else {
            sscanf(posStr.c_str(), "%f,%f,%f", &wposX, &wposY, &wposZ);
            wposA = 0;
        }
    } else {
        // No WPos - use MPos
        wposX = posX;
        wposY = posY;
        wposZ = posZ;
        wposA = posA;
    }
    
    // Parse WCO (Work Coordinate Offset) if present
    int wcoIndex = status.indexOf("WCO:");
    if (wcoIndex >= 0) {
        int endIndex = status.indexOf('|', wcoIndex);
        if (endIndex < 0) endIndex = status.indexOf('>', wcoIndex);
        String wcoStr = status.substring(wcoIndex + 4, endIndex);
        
        int commaCount = 0;
        for (int i = 0; i < wcoStr.length(); i++) {
            if (wcoStr.charAt(i) == ',') commaCount++;
        }
        
        if (commaCount >= 3) {
            sscanf(wcoStr.c_str(), "%f,%f,%f,%f", &wcoX, &wcoY, &wcoZ, &wcoA);
        } else {
            sscanf(wcoStr.c_str(), "%f,%f,%f", &wcoX, &wcoY, &wcoZ);
            wcoA = 0;
        }
        
        // Calculate WPos from MPos - WCO
        wposX = posX - wcoX;
        wposY = posY - wcoY;
        wposZ = posZ - wcoZ;
        wposA = posA - wcoA;
    }
    
    // Parse FS (Feed rate and Spindle speed)
    int fsIndex = status.indexOf("FS:");
    if (fsIndex >= 0) {
        int endIndex = status.indexOf('|', fsIndex);
        if (endIndex < 0) endIndex = status.indexOf('>', fsIndex);
        String fsStr = status.substring(fsIndex + 3, endIndex);
        sscanf(fsStr.c_str(), "%d,%d", &feedRate, &spindleRPM);
    }
    
    // Parse Ov (Overrides) if present
    int ovIndex = status.indexOf("Ov:");
    if (ovIndex >= 0) {
        int endIndex = status.indexOf('|', ovIndex);
        if (endIndex < 0) endIndex = status.indexOf('>', ovIndex);
        String ovStr = status.substring(ovIndex + 3, endIndex);
        sscanf(ovStr.c_str(), "%d,%d,%d", &feedOverride, &rapidOverride, &spindleOverride);
    }
}

// ========== Core Functions (Temperature, Fan, etc) ==========

// Non-blocking sensor sampling - call this repeatedly in loop()
void sampleSensorsNonBlocking() {
  if (millis() - lastAdcSample < 5) {
    return;  // Sample every 5ms
  }
  lastAdcSample = millis();

  // CYD NOTE: Only PSU voltage is ADC-based now (temperatures use DS18B20 OneWire)
  // Take one sample from PSU voltage sensor only
  adcSamples[4][adcSampleIndex] = analogRead(PSU_VOLT);  // Index 4 = PSU voltage

  // Move to next sample
  adcSampleIndex++;
  if (adcSampleIndex >= 10) {
    adcSampleIndex = 0;
    adcReady = true;  // PSU voltage sampling complete
  }
}

// Process averaged ADC readings (called when adcReady is true)
void processAdcReadings() {
  // CYD NOTE: Thermistor processing disabled - CYD uses DS18B20 OneWire sensors
  // TODO: Implement DS18B20 OneWire temperature reading for CYD
  // For now, set dummy temperature values to prevent display errors
  for (int sensor = 0; sensor < 4; sensor++) {
    temperatures[sensor] = 25.0;  // Placeholder: 25C room temperature
    peakTemps[sensor] = 25.0;
  }

  // Process PSU voltage
  uint32_t sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += adcSamples[4][i];
  }
  float adcValue = sum / 10.0;
  float measuredVoltage = (adcValue / ADC_RESOLUTION) * 3.3;
  psuVoltage = measuredVoltage * cfg.psu_voltage_cal;

  if (psuVoltage < psuMin && psuVoltage > 10.0) psuMin = psuVoltage;
  if (psuVoltage > psuMax) psuMax = psuVoltage;
}

float calculateThermistorTemp(float adcValue) {
  float voltage = (adcValue / ADC_RESOLUTION) * 3.3;
  if (voltage <= 0.01) return 0.0;  // Prevent division by zero

  float resistance = SERIES_RESISTOR * (3.3 / voltage - 1.0);
  float steinhart = resistance / THERMISTOR_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}

// Legacy function - now just calls non-blocking version
void readTemperatures() {
  // This function kept for compatibility but processing now happens in loop
}

void controlFan() {
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) {
      maxTemp = temperatures[i];
    }
  }
  
  if (maxTemp < cfg.temp_threshold_low) {
    fanSpeed = cfg.fan_min_speed;
  } else if (maxTemp > cfg.temp_threshold_high) {
    fanSpeed = cfg.fan_max_speed_limit;
  } else {
    fanSpeed = map(maxTemp * 100, cfg.temp_threshold_low * 100, 
                   cfg.temp_threshold_high * 100, 
                   cfg.fan_min_speed, cfg.fan_max_speed_limit);
  }
  
  uint8_t pwmValue = map(fanSpeed, 0, 100, 0, 255);
  ledcWrite(0, pwmValue);  // channel 0
}

void calculateRPM() {
  fanRPM = (tachCounter * 60) / 2;
  tachCounter = 0;
}

// readPSUVoltage removed - now handled by processAdcReadings()

void updateTempHistory() {
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) {
      maxTemp = temperatures[i];
    }
  }
  
  tempHistory[historyIndex] = maxTemp;
  historyIndex = (historyIndex + 1) % historySize;
}

// ========== Improved Display Functions ==========

void drawScreen() {
    switch(currentMode) {
        case MODE_MONITOR:
            // Try JSON layout first, fallback to legacy if not available
            if (monitorLayout.isValid) {
                Serial.println("[JSON] Drawing monitor from JSON layout");
                drawScreenFromLayout(monitorLayout);
            } else {
                Serial.println("[Legacy] Drawing monitor with legacy code");
                drawMonitorMode();
            }
            break;
            
        case MODE_ALIGNMENT:
            if (alignmentLayout.isValid) {
                drawScreenFromLayout(alignmentLayout);
            } else {
                drawAlignmentMode();
            }
            break;
            
        case MODE_GRAPH:
            if (graphLayout.isValid) {
                drawScreenFromLayout(graphLayout);
            } else {
                drawGraphMode();
            }
            break;
            
        case MODE_NETWORK:
            if (networkLayout.isValid) {
                drawScreenFromLayout(networkLayout);
            } else {
                drawNetworkMode();
            }
            break;
    }
}

void drawMonitorMode() {
  gfx.fillScreen(COLOR_BG);

  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(10, 6);
  gfx.print("FluidDash");

  // DateTime in header (right side)
  char buffer[40];
  if (rtcAvailable) {
    DateTime now = rtc.now();
    sprintf(buffer, "%s %02d  %02d:%02d:%02d",
            getMonthName(now.month()), now.day(), now.hour(), now.minute(), now.second());
  } else {
    sprintf(buffer, "No RTC");
  }
  gfx.setCursor(270, 6);
  gfx.print(buffer);

  // Dividers
  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  gfx.drawFastHLine(0, 175, SCREEN_WIDTH, COLOR_LINE);
  gfx.drawFastVLine(240, 25, 150, COLOR_LINE);

  // Left section - Driver temperatures
  gfx.setTextSize(1);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 30);
  gfx.print("DRIVERS:");

  const char* labels[] = {"X:", "YL:", "YR:", "Z:"};
  for (int i = 0; i < 4; i++) {
    gfx.setCursor(10, 50 + i * 30);
    gfx.setTextColor(COLOR_TEXT);
    gfx.print(labels[i]);

    // Current temp
    gfx.setTextSize(2);
    gfx.setTextColor(temperatures[i] > cfg.temp_threshold_high ? COLOR_WARN : COLOR_VALUE);
    gfx.setCursor(50, 47 + i * 30);
    sprintf(buffer, "%d%s", (int)temperatures[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);

    // Peak temp (smaller, to the right)
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(140, 52 + i * 30);
    sprintf(buffer, "pk:%d%s", (int)peakTemps[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);

    gfx.setTextSize(1);
  }

  // Status section
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 185);
  gfx.print("STATUS:");

  gfx.setCursor(10, 200);
  gfx.setTextColor(COLOR_LINE);
  sprintf(buffer, "Fan: %d%% (%dRPM)", fanSpeed, fanRPM);
  gfx.print(buffer);

  gfx.setCursor(10, 215);
  sprintf(buffer, "PSU: %.1fV", psuVoltage);
  gfx.print(buffer);

  gfx.setCursor(10, 230);
  if (fluidncConnected) {
    if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
    else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
    else gfx.setTextColor(COLOR_VALUE);
    sprintf(buffer, "FluidNC: %s", machineState.c_str());
  } else {
    gfx.setTextColor(COLOR_WARN);
    sprintf(buffer, "FluidNC: Disconnected");
  }
  gfx.print(buffer);

  // Coordinates
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 250);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "WCS: X:%.3f Y:%.3f Z:%.3f", wposX, wposY, wposZ);
  } else {
    sprintf(buffer, "WCS: X:%.2f Y:%.2f Z:%.2f", wposX, wposY, wposZ);
  }
  gfx.print(buffer);

  gfx.setCursor(10, 265);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "MCS: X:%.3f Y:%.3f Z:%.3f", posX, posY, posZ);
  } else {
    sprintf(buffer, "MCS: X:%.2f Y:%.2f Z:%.2f", posX, posY, posZ);
  }
  gfx.print(buffer);

  // Right section - Temperature graph
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(250, 30);
  gfx.print("TEMP HISTORY");

  if (cfg.show_temp_graph) {
    char graphLabel[40];
    if (cfg.graph_timespan_seconds >= 60) {
      sprintf(graphLabel, "(%d min)", cfg.graph_timespan_seconds / 60);
    } else {
      sprintf(graphLabel, "(%d sec)", cfg.graph_timespan_seconds);
    }
    gfx.setCursor(250, 40);
    gfx.setTextColor(COLOR_LINE);
    gfx.print(graphLabel);

    // Draw the temperature history graph
    drawTempGraph(250, 55, 220, 110);
  }
}

void updateDisplay() {
    if (currentMode == MODE_MONITOR) {
        // Use JSON dynamic update if available, otherwise legacy
        if (monitorLayout.isValid) {
            updateDynamicElements(monitorLayout);
        } else {
            updateMonitorMode();
        }
    } else if (currentMode == MODE_ALIGNMENT) {
        if (alignmentLayout.isValid) {
            updateDynamicElements(alignmentLayout);
        } else {
            updateAlignmentMode();
        }
    } else if (currentMode == MODE_GRAPH) {
        if (graphLayout.isValid) {
            updateDynamicElements(graphLayout);
        } else {
            updateGraphMode();
        }
    } else if (currentMode == MODE_NETWORK) {
        if (networkLayout.isValid) {
            updateDynamicElements(networkLayout);
        } else {
            updateNetworkMode();
        }
    }
}

void updateMonitorMode() {
  // LovyanGFX version - only update dynamic parts (clear area, then redraw)
  char buffer[80];

  // Update DateTime in header
  if (rtcAvailable) {
    DateTime now = rtc.now();
    sprintf(buffer, "%s %02d  %02d:%02d:%02d",
            getMonthName(now.month()), now.day(), now.hour(), now.minute(), now.second());
  } else {
    sprintf(buffer, "No RTC");
  }
  gfx.fillRect(270, 0, 210, 25, COLOR_HEADER);
  gfx.setTextSize(2);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(270, 6);
  gfx.print(buffer);

  // Update temperature values and peaks
  for (int i = 0; i < 4; i++) {
    // Clear the temperature display area for this driver
    gfx.fillRect(50, 47 + i * 30, 180, 20, COLOR_BG);

    // Current temp
    gfx.setTextSize(2);
    gfx.setTextColor(temperatures[i] > cfg.temp_threshold_high ? COLOR_WARN : COLOR_VALUE);
    gfx.setCursor(50, 47 + i * 30);
    sprintf(buffer, "%d%s", (int)temperatures[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);

    // Peak temp
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(140, 52 + i * 30);
    sprintf(buffer, "pk:%d%s", (int)peakTemps[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);
  }

  // Update status section
  gfx.setTextSize(1);

  // Fan
  gfx.fillRect(10, 200, 220, 10, COLOR_BG);
  gfx.setTextColor(COLOR_LINE);
  gfx.setCursor(10, 200);
  sprintf(buffer, "Fan: %d%% (%dRPM)", fanSpeed, fanRPM);
  gfx.print(buffer);

  // PSU
  gfx.fillRect(10, 215, 220, 10, COLOR_BG);
  gfx.setCursor(10, 215);
  gfx.setTextColor(COLOR_LINE);
  sprintf(buffer, "PSU: %.1fV", psuVoltage);
  gfx.print(buffer);

  // FluidNC Status
  gfx.fillRect(10, 230, 220, 10, COLOR_BG);
  gfx.setCursor(10, 230);
  if (fluidncConnected) {
    if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
    else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
    else gfx.setTextColor(COLOR_VALUE);
    sprintf(buffer, "FluidNC: %s", machineState.c_str());
  } else {
    gfx.setTextColor(COLOR_WARN);
    sprintf(buffer, "FluidNC: Disconnected");
  }
  gfx.print(buffer);

  // WCS Coordinates
  gfx.fillRect(10, 250, 220, 10, COLOR_BG);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 250);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "WCS: X:%.3f Y:%.3f Z:%.3f", wposX, wposY, wposZ);
  } else {
    sprintf(buffer, "WCS: X:%.2f Y:%.2f Z:%.2f", wposX, wposY, wposZ);
  }
  gfx.print(buffer);

  // MCS Coordinates
  gfx.fillRect(10, 265, 220, 10, COLOR_BG);
  gfx.setCursor(10, 265);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "MCS: X:%.3f Y:%.3f Z:%.3f", posX, posY, posZ);
  } else {
    sprintf(buffer, "MCS: X:%.2f Y:%.2f Z:%.2f", posX, posY, posZ);
  }
  gfx.print(buffer);

  // Update temperature graph (if enabled)
  if (cfg.show_temp_graph) {
    drawTempGraph(250, 55, 220, 110);
  }
}

void drawAlignmentMode() {
  gfx.fillScreen(COLOR_BG);
  
  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(140, 6);
  gfx.print("ALIGNMENT MODE");
  
  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  
  // Title
  gfx.setTextSize(2);
  gfx.setTextColor(COLOR_HEADER);
  gfx.setCursor(150, 40);
  gfx.print("WORK POSITION");
  
  // Detect if 4-axis machine (if A-axis is non-zero or moving)
  bool has4Axes = (posA != 0 || wposA != 0);
  
  if (has4Axes) {
    // 4-AXIS DISPLAY - Slightly smaller to fit all axes
    gfx.setTextSize(4);
    gfx.setTextColor(COLOR_VALUE);
    
    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "X:%9.3f");
    } else {
      strcpy(coordFormat, "X:%8.2f");
    }
    
    gfx.setCursor(40, 75);
    gfx.printf(coordFormat, wposX);
    
    coordFormat[0] = 'Y';
    gfx.setCursor(40, 120);
    gfx.printf(coordFormat, wposY);
    
    coordFormat[0] = 'Z';
    gfx.setCursor(40, 165);
    gfx.printf(coordFormat, wposZ);
    
    coordFormat[0] = 'A';
    gfx.setCursor(40, 210);
    gfx.printf(coordFormat, wposA);
    
    // Small info footer for 4-axis
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(10, 265);
    gfx.printf("Machine: X:%.1f Y:%.1f Z:%.1f A:%.1f", posX, posY, posZ, posA);
  } else {
    // 3-AXIS DISPLAY - Original large size
    gfx.setTextSize(5);
    gfx.setTextColor(COLOR_VALUE);
    
    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "X:%9.3f");
    } else {
      strcpy(coordFormat, "X:%8.2f");
    }
    
    gfx.setCursor(40, 90);
    gfx.printf(coordFormat, wposX);
    
    coordFormat[0] = 'Y';
    gfx.setCursor(40, 145);
    gfx.printf(coordFormat, wposY);
    
    coordFormat[0] = 'Z';
    gfx.setCursor(40, 200);
    gfx.printf(coordFormat, wposZ);
    
    // Small info footer for 3-axis
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(10, 270);
    gfx.printf("Machine: X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  }
  
  // Status line (same for both)
  gfx.setCursor(10, 285);
  if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
  else gfx.setTextColor(COLOR_VALUE);
  gfx.printf("Status: %s", machineState.c_str());
  
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) maxTemp = temperatures[i];
  }
  
  gfx.setTextColor(maxTemp > cfg.temp_threshold_high ? COLOR_WARN : COLOR_LINE);
  gfx.setCursor(10, 300);
  gfx.printf("Temps:%.0fC  Fan:%d%%  PSU:%.1fV", maxTemp, fanSpeed, psuVoltage);
}


void updateAlignmentMode() {
  // Detect if 4-axis machine
  bool has4Axes = (posA != 0 || wposA != 0);
  
  if (has4Axes) {
    // 4-AXIS UPDATE
    gfx.setTextSize(4);
    gfx.setTextColor(COLOR_VALUE);
    
    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "%9.3f");
    } else {
      strcpy(coordFormat, "%8.2f");
    }
    
    // Update X
    gfx.fillRect(140, 75, 330, 32, COLOR_BG);
    gfx.setCursor(140, 75);
    gfx.printf(coordFormat, wposX);
    
    // Update Y
    gfx.fillRect(140, 120, 330, 32, COLOR_BG);
    gfx.setCursor(140, 120);
    gfx.printf(coordFormat, wposY);
    
    // Update Z
    gfx.fillRect(140, 165, 330, 32, COLOR_BG);
    gfx.setCursor(140, 165);
    gfx.printf(coordFormat, wposZ);
    
    // Update A
    gfx.fillRect(140, 210, 330, 32, COLOR_BG);
    gfx.setCursor(140, 210);
    gfx.printf(coordFormat, wposA);
    
    // Update footer
    gfx.setTextSize(1);
    gfx.fillRect(90, 265, 390, 40, COLOR_BG);
    
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(90, 265);
    gfx.printf("X:%.1f Y:%.1f Z:%.1f A:%.1f", posX, posY, posZ, posA);
  } else {
    // 3-AXIS UPDATE - Original code
    gfx.setTextSize(5);
    gfx.setTextColor(COLOR_VALUE);
    
    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "%9.3f");
    } else {
      strcpy(coordFormat, "%8.2f");
    }
    
    gfx.fillRect(150, 90, 320, 38, COLOR_BG);
    gfx.setCursor(150, 90);
    gfx.printf(coordFormat, wposX);
    
    gfx.fillRect(150, 145, 320, 38, COLOR_BG);
    gfx.setCursor(150, 145);
    gfx.printf(coordFormat, wposY);
    
    gfx.fillRect(150, 200, 320, 38, COLOR_BG);
    gfx.setCursor(150, 200);
    gfx.printf(coordFormat, wposZ);
    
    // Update footer
    gfx.setTextSize(1);
    gfx.fillRect(90, 270, 390, 35, COLOR_BG);
    
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(90, 270);
    gfx.printf("X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  }
  
  // Update status (same for both)
  gfx.setCursor(80, 285);
  if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
  else gfx.setTextColor(COLOR_VALUE);
  gfx.printf("%s", machineState.c_str());
  
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) maxTemp = temperatures[i];
  }
  
  gfx.setTextColor(maxTemp > cfg.temp_threshold_high ? COLOR_WARN : COLOR_LINE);
  gfx.setCursor(90, 300);
  gfx.printf("%.0fC  Fan:%d%%  PSU:%.1fV", maxTemp, fanSpeed, psuVoltage);
}

void drawGraphMode() {
  gfx.fillScreen(COLOR_BG);
  
  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(100, 6);
  gfx.print("TEMPERATURE HISTORY");
  
  char timeLabel[40];
  if (cfg.graph_timespan_seconds >= 60) {
    sprintf(timeLabel, " - %d minutes", cfg.graph_timespan_seconds / 60);
  } else {
    sprintf(timeLabel, " - %d seconds", cfg.graph_timespan_seconds);
  }
  gfx.setTextSize(1);
  gfx.setCursor(330, 10);
  gfx.print(timeLabel);
  
  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  
  // Full screen graph
  drawTempGraph(20, 40, 440, 270);
}

void updateGraphMode() {
  // Redraw graph
  drawTempGraph(20, 40, 440, 270);
}

void drawNetworkMode() {
  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(120, 6);
  gfx.print("NETWORK STATUS");

  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);

  gfx.setTextSize(2);
  gfx.setTextColor(COLOR_TEXT);

  if (inAPMode) {
    // AP Mode display
    gfx.setCursor(60, 50);
    gfx.setTextColor(COLOR_WARN);
    gfx.print("WiFi Config Mode Active");

    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(10, 90);
    gfx.print("1. Connect to WiFi network:");

    gfx.setTextSize(2);
    gfx.setTextColor(COLOR_VALUE);
    gfx.setCursor(40, 110);
    gfx.print("FluidDash-Setup");

    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(10, 145);
    gfx.print("2. Open browser and go to:");

    gfx.setTextSize(2);
    gfx.setTextColor(COLOR_VALUE);
    gfx.setCursor(80, 165);
    gfx.print("http://192.168.4.1");

    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(10, 200);
    gfx.print("3. Configure your WiFi settings");

    gfx.setCursor(10, 230);
    gfx.setTextColor(COLOR_LINE);
    gfx.print("Temperature monitoring continues in background");

    // Show to exit AP mode
    gfx.setTextColor(COLOR_ORANGE);
    gfx.setCursor(10, 270);
    gfx.print("Press button briefly to return to monitoring");

  } else {
    // Normal network status display
    if (WiFi.status() == WL_CONNECTED) {
      gfx.setCursor(130, 50);
      gfx.setTextColor(COLOR_GOOD);
      gfx.print("WiFi Connected");

      gfx.setTextSize(1);
      gfx.setTextColor(COLOR_TEXT);

      gfx.setCursor(10, 90);
      gfx.print("SSID:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 90);
      gfx.print(WiFi.SSID());

      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 115);
      gfx.print("IP Address:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 115);
      gfx.print(WiFi.localIP());

      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 140);
      gfx.print("Signal:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 140);
      int rssi = WiFi.RSSI();
      gfx.printf("%d dBm", rssi);

      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 165);
      gfx.print("mDNS:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 165);
      gfx.printf("http://%s.local", cfg.device_name);

      if (fluidncConnected) {
        gfx.setTextColor(COLOR_TEXT);
        gfx.setCursor(10, 190);
        gfx.print("FluidNC:");
        gfx.setTextColor(COLOR_GOOD);
        gfx.setCursor(80, 190);
        gfx.print("Connected");
      } else {
        gfx.setTextColor(COLOR_TEXT);
        gfx.setCursor(10, 190);
        gfx.print("FluidNC:");
        gfx.setTextColor(COLOR_WARN);
        gfx.setCursor(80, 190);
        gfx.print("Disconnected");
      }

    } else {
      gfx.setCursor(120, 50);
      gfx.setTextColor(COLOR_WARN);
      gfx.print("WiFi Not Connected");

      gfx.setTextSize(1);
      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 100);
      gfx.print("Temperature monitoring active (standalone mode)");

      gfx.setCursor(10, 130);
      gfx.setTextColor(COLOR_ORANGE);
      gfx.print("To configure WiFi:");
    }

    // Instructions for entering AP mode
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(10, 250);
    gfx.print("Hold button for 10 seconds to enter WiFi");
    gfx.setCursor(10, 265);
    gfx.print("configuration mode");
  }
}

void updateNetworkMode() {
  // Update time in header if needed - but network info is mostly static
  // Could add dynamic signal strength updates here
}

void drawTempGraph(int x, int y, int w, int h) {
  gfx.fillRect(x, y, w, h, COLOR_BG);
  gfx.drawRect(x, y, w, h, COLOR_LINE);
  
  float minTemp = 10.0;
  float maxTemp = 60.0;
  
  // Draw temperature line
  for (int i = 1; i < historySize; i++) {
    int idx1 = (historyIndex + i - 1) % historySize;
    int idx2 = (historyIndex + i) % historySize;
    
    float temp1 = tempHistory[idx1];
    float temp2 = tempHistory[idx2];
    
    int x1 = x + ((i - 1) * w / historySize);
    int y1 = y + h - ((temp1 - minTemp) / (maxTemp - minTemp) * h);
    int x2 = x + (i * w / historySize);
    int y2 = y + h - ((temp2 - minTemp) / (maxTemp - minTemp) * h);
    
    y1 = constrain(y1, y, y + h);
    y2 = constrain(y2, y, y + h);
    
    // Color based on temperature
    uint16_t color;
    if (temp2 > cfg.temp_threshold_high) color = COLOR_WARN;
    else if (temp2 > cfg.temp_threshold_low) color = COLOR_ORANGE;
    else color = COLOR_GOOD;
    
    gfx.drawLine(x1, y1, x2, y2, color);
  }
  
  // Scale markers
  gfx.setTextSize(1);
  gfx.setTextColor(COLOR_LINE);
  gfx.setCursor(x + 3, y + 2);
  gfx.print("60");
  gfx.setCursor(x + 3, y + h / 2 - 5);
  gfx.print("35");
  gfx.setCursor(x + 3, y + h - 10);
  gfx.print("10");
}

// ========== Button Handling ==========

void handleButton() {
  bool currentState = (digitalRead(BTN_MODE) == LOW);
  
  if (currentState && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = millis();
  }
  else if (!currentState && buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressStart;
    buttonPressed = false;
    
    if (pressDuration >= 5000) {
      enterSetupMode();
    } else if (pressDuration < 1000) {
      cycleDisplayMode();
    }
  }
  else if (buttonPressed && (millis() - buttonPressStart >= 2000)) {
    showHoldProgress();
  }
}

void cycleDisplayMode() {
  currentMode = (DisplayMode)((currentMode + 1) % 4);  // Now we have 4 modes
  drawScreen();
  
  // Flash mode name
  gfx.fillRect(180, 140, 120, 40, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(190, 150);
  
  switch(currentMode) {
    case MODE_MONITOR: gfx.print("MONITOR"); break;
    case MODE_ALIGNMENT: gfx.print("ALIGNMENT"); break;
    case MODE_GRAPH: gfx.print("GRAPH"); break;
  }
  
  delay(800);
  drawScreen();
}

void showHoldProgress() {
  unsigned long elapsed = millis() - buttonPressStart;
  int progress = map(elapsed, 2000, 5000, 0, 100);
  progress = constrain(progress, 0, 100);
  
  gfx.fillRect(140, 280, 200, 30, COLOR_BG);
  gfx.drawRect(140, 280, 200, 30, COLOR_TEXT);
  gfx.setTextColor(COLOR_WARN);
  gfx.setTextSize(1);
  gfx.setCursor(145, 285);
  gfx.print("Hold for Setup...");
  
  int barWidth = map(progress, 0, 100, 0, 190);
  gfx.fillRect(145, 295, barWidth, 10, COLOR_WARN);
  
  gfx.setCursor(145, 307);
  gfx.print(10 - (elapsed / 1000));
  gfx.print(" sec");
}

void enterSetupMode() {
  Serial.println("Entering WiFi configuration AP mode...");

  // Stop any existing WiFi connection
  WiFi.disconnect();
  delay(100);

  // Start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FluidDash-Setup");
  inAPMode = true;

  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());

  // Show AP mode screen
  currentMode = MODE_NETWORK;
  drawScreen();

  Serial.println("WiFi configuration AP active. Device will continue monitoring.");
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

const char* getMonthName(int month) {
  const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return months[month];
}
