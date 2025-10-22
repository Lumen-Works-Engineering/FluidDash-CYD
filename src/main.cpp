/*
 * CNC GUARDIAN v07 - Full Configuration System
 * - WiFiManager for initial setup
 * - Preferences for persistent storage
 * - Web interface for all settings
 * - Configurable graph timespan
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiManager.h>
#if __has_include(<WebSocketsClient.h>)
#include <WebSocketsClient.h>
#else
// Minimal stub for WebSocketsClient to allow compilation when the library is not installed.
// This stub implements only the parts used by this project (no actual network functionality).
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

// Hardware pins (unchanged)
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_MOSI 23
#define TFT_SCK  18
#define FAN_PWM   25
#define FAN_TACH  26
#define THERM_X   32
#define THERM_YL  33
#define THERM_YR  34
#define THERM_Z   35
#define PSU_VOLT  36
#define BTN_MODE  14

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
  strcpy(cfg.device_name, "cnc-guardian");
  strcpy(cfg.fluidnc_ip, "192.168.73.13");
  cfg.fluidnc_port = 81;
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
  cfg.psu_alert_low = 22.0;
  cfg.psu_alert_high = 26.0;

  cfg.brightness = 255;
  cfg.default_mode = MODE_MONITOR;
  cfg.show_machine_coords = true;
  cfg.show_temp_graph = true;
  cfg.coord_decimal_places = 2;

  cfg.graph_timespan_seconds = 300;  // 5 minutes default
  cfg.graph_update_interval = 5;      // 5 seconds per point

  cfg.use_fahrenheit = false;
  cfg.use_inches = false;

  cfg.enable_logging = false;
  cfg.status_update_rate = 200;
}

// Objects
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7796(bus, TFT_RST, 1, false);  // rotation: 0=0°, 1=90°, 2=180°, 3=270°
RTC_DS3231 rtc;
WebSocketsClient webSocket;
Preferences prefs;
AsyncWebServer server(80);
WiFiManager wm;

// Runtime variables
DisplayMode currentMode;
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

// WiFi AP mode flag
bool inAPMode = false;

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

// ========== Function Prototypes ==========
void loadConfig();
void saveConfig();
void allocateHistoryBuffer();
void setupWiFiManager();
void setupWebServer();
String getMainHTML();
String getSettingsHTML();
String getAdminHTML();
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

void IRAM_ATTR tachISR() {
  tachCounter++;
}

void setup() {
  Serial.begin(115200);
  Serial.println("CNC Guardian v07 - Starting...");

  // Initialize default configuration
  initDefaultConfig();

  // Enable watchdog timer (10 seconds)
  enableLoopWDT();
  Serial.println("Watchdog timer enabled (10s timeout)");

  // Initialize display (feed watchdog before long operation)
  feedLoopWDT();
  Serial.println("Initializing display...");
  if (gfx->begin()) {
    Serial.println("Display initialized OK");
    gfx->fillScreen(COLOR_BG);
    showSplashScreen();
  } else {
    Serial.println("Display initialization failed - continuing without display");
  }
  feedLoopWDT();

  // Initialize hardware
  Wire.begin(21, 22);
  rtc.begin();
  pinMode(BTN_MODE, INPUT_PULLUP);

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
  WiFi.mode(WIFI_STA);
  WiFi.begin();  // Try to connect with saved credentials

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

    if (millis() - lastStatusRequest >= cfg.status_update_rate && fluidncConnected) {
      if (!webSocket.sendTXT("?\n")) {
        Serial.println("[FluidNC] Warning: Failed to send status request");
      }
      lastStatusRequest = millis();
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
  prefs.begin("cnc-guardian", true);
  
  strlcpy(cfg.device_name, prefs.getString("dev_name", "cnc-guardian").c_str(), 32);
  strlcpy(cfg.fluidnc_ip, prefs.getString("fnc_ip", "192.168.73.13").c_str(), 16);
  cfg.fluidnc_port = prefs.getUShort("fnc_port", 81);
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
  prefs.begin("cnc-guardian", false);
  
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

// Continue in next message due to length...
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
  <title>CNC Guardian</title>
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
    <h1>🛡️ CNC Guardian</h1>
    
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
      <button onclick='restart()'>🔄 Restart Device</button>
      <button onclick='resetWiFi()'>📡 Reset WiFi</button>
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
          .then(() => alert('WiFi reset. Connect to CNC-Guardian-Setup network.'));
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
  <title>Settings - CNC Guardian</title>
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
  <title>Admin - CNC Guardian</title>
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
  Serial.printf("[FluidNC] Attempting to connect to %s:%d\n", cfg.fluidnc_ip, cfg.fluidnc_port);
  webSocket.begin(cfg.fluidnc_ip, cfg.fluidnc_port, "/");
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
      Serial.println("[FluidNC] Disconnected");
      fluidncConnected = false;
      machineState = "OFFLINE";
      break;

    case WStype_CONNECTED: {
      Serial.println("[FluidNC] Connected");
      fluidncConnected = true;
      machineState = "IDLE";
      String cmd = "$Report/Interval=" + String(cfg.status_update_rate) + "\n";
      if (!webSocket.sendTXT(cmd)) {
        Serial.println("[FluidNC] Warning: Failed to send report interval command");
      }
      break;
    }

    case WStype_TEXT: {
      String msg = String((char*)payload);
      if (msg.startsWith("<")) {
        parseFluidNCStatus(msg);
      }
      break;
    }

    case WStype_ERROR:
      Serial.println("[FluidNC] WebSocket Error");
      break;
  }
}

void parseFluidNCStatus(String status) {
  String oldState = machineState;
  
  // Parse state
  if (status.indexOf("Idle") > 0) machineState = "IDLE";
  else if (status.indexOf("Run") > 0) machineState = "RUN";
  else if (status.indexOf("Hold") > 0) machineState = "HOLD";
  else if (status.indexOf("Alarm") > 0) machineState = "ALARM";
  else if (status.indexOf("Home") > 0) machineState = "HOME";
  else if (status.indexOf("Jog") > 0) machineState = "JOG";
  
  // Job tracking
  if (oldState != "RUN" && machineState == "RUN") {
    jobStartTime = millis();
    isJobRunning = true;
  }
  if (oldState == "RUN" && machineState != "RUN") {
    isJobRunning = false;
  }
  
  // Parse MPos
  int mposIndex = status.indexOf("MPos:");
  if (mposIndex > 0) {
    int endIndex = status.indexOf("|", mposIndex);
    String posStr = status.substring(mposIndex + 5, endIndex);
    sscanf(posStr.c_str(), "%f,%f,%f,%f", &posX, &posY, &posZ, &posA);
  }
  
  // Parse WPos
  int wposIndex = status.indexOf("WPos:");
  if (wposIndex > 0) {
    int endIndex = status.indexOf("|", wposIndex);
    String posStr = status.substring(wposIndex + 5, endIndex);
    sscanf(posStr.c_str(), "%f,%f,%f,%f", &wposX, &wposY, &wposZ, &wposA);
  } else {
    wposX = posX;
    wposY = posY;
    wposZ = posZ;
    wposA = posA;
  }
  
  // Parse FS
  int fsIndex = status.indexOf("FS:");
  if (fsIndex > 0) {
    int endIndex = status.indexOf("|", fsIndex);
    String fsStr = status.substring(fsIndex + 3, endIndex);
    sscanf(fsStr.c_str(), "%d,%d", &feedRate, &spindleRPM);
  }
}

// ========== Core Functions (Temperature, Fan, etc) ==========

// Non-blocking sensor sampling - call this repeatedly in loop()
void sampleSensorsNonBlocking() {
  if (millis() - lastAdcSample < 5) {
    return;  // Sample every 5ms
  }
  lastAdcSample = millis();

  // Take one sample from current sensor
  uint8_t pins[] = {THERM_X, THERM_YL, THERM_YR, THERM_Z, PSU_VOLT};
  adcSamples[adcCurrentSensor][adcSampleIndex] = analogRead(pins[adcCurrentSensor]);

  // Move to next sample or next sensor
  adcSampleIndex++;
  if (adcSampleIndex >= 10) {
    adcSampleIndex = 0;
    adcCurrentSensor++;

    if (adcCurrentSensor >= 5) {
      adcCurrentSensor = 0;
      adcReady = true;  // All sensors sampled
    }
  }
}

// Process averaged ADC readings (called when adcReady is true)
void processAdcReadings() {
  // Process thermistors
  for (int sensor = 0; sensor < 4; sensor++) {
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) {
      sum += adcSamples[sensor][i];
    }
    float adcValue = sum / 10.0;
    temperatures[sensor] = calculateThermistorTemp(adcValue);

    // Apply calibration offsets
    float offsets[] = {cfg.temp_offset_x, cfg.temp_offset_yl, cfg.temp_offset_yr, cfg.temp_offset_z};
    temperatures[sensor] += offsets[sensor];

    // Update peaks
    if (temperatures[sensor] > peakTemps[sensor]) {
      peakTemps[sensor] = temperatures[sensor];
    }
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
  gfx->fillScreen(COLOR_BG);

  switch(currentMode) {
    case MODE_MONITOR:
      drawMonitorMode();
      break;
    case MODE_ALIGNMENT:
      drawAlignmentMode();
      break;
    case MODE_GRAPH:
      drawGraphMode();
      break;
    case MODE_NETWORK:
      drawNetworkMode();
      break;
  }
}

void drawMonitorMode() {
  // Header
  gfx->fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(10, 6);
  gfx->print("CNC GUARDIAN");
  
  // Dividers
  gfx->drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  gfx->drawFastHLine(0, 175, SCREEN_WIDTH, COLOR_LINE);
  gfx->drawFastVLine(240, 25, 150, COLOR_LINE);
  
  // Labels
  gfx->setTextSize(1);
  gfx->setCursor(10, 30);
  gfx->print("DRIVERS:");
  
  const char* labels[] = {"X:", "YL:", "YR:", "Z:"};
  for (int i = 0; i < 4; i++) {
    gfx->setCursor(10, 50 + i * 22);
    gfx->print(labels[i]);
  }
  
  gfx->setCursor(10, 185);
  gfx->print("STATUS:");
  
  gfx->setCursor(250, 30);
  gfx->print("TEMP HISTORY");
  
  if (cfg.show_temp_graph) {
    char graphLabel[40];
    if (cfg.graph_timespan_seconds >= 60) {
      sprintf(graphLabel, "(%d min)", cfg.graph_timespan_seconds / 60);
    } else {
      sprintf(graphLabel, "(%d sec)", cfg.graph_timespan_seconds);
    }
    gfx->setCursor(250, 40);
    gfx->print(graphLabel);
  }
}

void updateDisplay() {
  if (currentMode == MODE_MONITOR) {
    updateMonitorMode();
  } else if (currentMode == MODE_ALIGNMENT) {
    updateAlignmentMode();
  } else if (currentMode == MODE_GRAPH) {
    updateGraphMode();
  } else if (currentMode == MODE_NETWORK) {
    updateNetworkMode();
  }
}

void updateMonitorMode() {
  DateTime now = rtc.now();
  
  // Update header time/date
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_TEXT);
  gfx->fillRect(320, 6, 155, 14, COLOR_HEADER);
  gfx->setCursor(320, 6);
  char timeStr[30];
  sprintf(timeStr, "%s %02d  %02d:%02d:%02d", 
          getMonthName(now.month()), now.day(), now.hour(), now.minute(), now.second());
  gfx->print(timeStr);
  
  // Update temps (compact)
  gfx->setTextSize(2);
  for (int i = 0; i < 4; i++) {
    gfx->fillRect(40, 48 + i * 22, 190, 18, COLOR_BG);
    
    gfx->setTextColor(COLOR_VALUE);
    gfx->setCursor(40, 50 + i * 22);
    gfx->printf("%dC", (int)temperatures[i]);
    
    gfx->setTextColor(COLOR_WARN);
    gfx->setCursor(100, 50 + i * 22);
    gfx->printf("%dC", (int)peakTemps[i]);
  }
  
  // Fan & PSU status (one line)
  gfx->fillRect(250, 155, 220, 18, COLOR_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_VALUE);
  gfx->setCursor(250, 160);
  gfx->printf("Fan:%d%%", fanSpeed);
  
  gfx->setCursor(320, 160);
  if (psuVoltage < cfg.psu_alert_low || psuVoltage > cfg.psu_alert_high) {
    gfx->setTextColor(COLOR_WARN);
  } else {
    gfx->setTextColor(COLOR_GOOD);
  }
  gfx->printf("PSU:%.1fV", psuVoltage);
  
  // Machine status - large
  gfx->fillRect(10, 205, 220, 30, COLOR_BG);
  gfx->setTextSize(3);
  
  if (machineState == "RUN") gfx->setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx->setTextColor(COLOR_WARN);
  else if (machineState == "OFFLINE") gfx->setTextColor(COLOR_ORANGE);
  else gfx->setTextColor(COLOR_VALUE);
  
  gfx->setCursor(80, 210);
  gfx->print(machineState);
  
  // WORK coordinates - large, single line
  gfx->fillRect(10, 245, 220, 35, COLOR_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setCursor(10, 245);
  gfx->print("WORK:");
  
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_VALUE);
  gfx->setCursor(10, 260);
  
  char coordFormat[20];
  if (cfg.coord_decimal_places == 3) {
    sprintf(coordFormat, "X:%%7.3f Y:%%7.3f");
  } else {
    sprintf(coordFormat, "X:%%6.2f Y:%%6.2f");
  }
  gfx->printf(coordFormat, wposX, wposY);
  
  gfx->setCursor(10, 285);
  if (cfg.coord_decimal_places == 3) {
    gfx->printf("Z:%7.3f", wposZ);
  } else {
    gfx->printf("Z:%6.2f", wposZ);
  }
  
  // Machine coordinates - small, if enabled
  if (cfg.show_machine_coords) {
    gfx->fillRect(10, 305, 220, 10, COLOR_BG);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_LINE);
    gfx->setCursor(10, 305);
    gfx->printf("Mach: X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  }
  
  // Temperature graph
  if (cfg.show_temp_graph) {
    drawTempGraph(250, 55, 220, 95);
  }
}

void drawAlignmentMode() {
  gfx->fillScreen(COLOR_BG);
  
  // Header
  gfx->fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(140, 6);
  gfx->print("ALIGNMENT MODE");
  
  gfx->drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  
  // Title
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_HEADER);
  gfx->setCursor(150, 40);
  gfx->print("WORK POSITION");
  
  // Extra-large coordinates
  gfx->setTextSize(5);
  gfx->setTextColor(COLOR_VALUE);
  
  char coordFormat[20];
  if (cfg.coord_decimal_places == 3) {
    strcpy(coordFormat, "X:%9.3f");
  } else {
    strcpy(coordFormat, "X:%8.2f");
  }
  
  gfx->setCursor(40, 90);
  gfx->printf(coordFormat, wposX);
  
  coordFormat[0] = 'Y';
  gfx->setCursor(40, 145);
  gfx->printf(coordFormat, wposY);
  
  coordFormat[0] = 'Z';
  gfx->setCursor(40, 200);
  gfx->printf(coordFormat, wposZ);
  
  // Small info footer
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_LINE);
  gfx->setCursor(10, 270);
  gfx->printf("Machine: X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  
  gfx->setCursor(10, 285);
  if (machineState == "RUN") gfx->setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx->setTextColor(COLOR_WARN);
  else gfx->setTextColor(COLOR_VALUE);
  gfx->printf("Status: %s", machineState.c_str());
  
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) maxTemp = temperatures[i];
  }
  
  gfx->setTextColor(maxTemp > cfg.temp_threshold_high ? COLOR_WARN : COLOR_LINE);
  gfx->setCursor(10, 300);
  gfx->printf("Temps:%.0fC  Fan:%d%%  PSU:%.1fV", maxTemp, fanSpeed, psuVoltage);
}

void updateAlignmentMode() {
  // Update large coordinates
  gfx->setTextSize(5);
  gfx->setTextColor(COLOR_VALUE);
  
  char coordFormat[20];
  if (cfg.coord_decimal_places == 3) {
    strcpy(coordFormat, "%9.3f");
  } else {
    strcpy(coordFormat, "%8.2f");
  }
  
  gfx->fillRect(150, 90, 320, 38, COLOR_BG);
  gfx->setCursor(150, 90);
  gfx->printf(coordFormat, wposX);
  
  gfx->fillRect(150, 145, 320, 38, COLOR_BG);
  gfx->setCursor(150, 145);
  gfx->printf(coordFormat, wposY);
  
  gfx->fillRect(150, 200, 320, 38, COLOR_BG);
  gfx->setCursor(150, 200);
  gfx->printf(coordFormat, wposZ);
  
  // Update footer
  gfx->setTextSize(1);
  gfx->fillRect(90, 270, 390, 45, COLOR_BG);
  
  gfx->setTextColor(COLOR_LINE);
  gfx->setCursor(90, 270);
  gfx->printf("X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  
  gfx->setCursor(80, 285);
  if (machineState == "RUN") gfx->setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx->setTextColor(COLOR_WARN);
  else gfx->setTextColor(COLOR_VALUE);
  gfx->printf("%s", machineState.c_str());
  
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) maxTemp = temperatures[i];
  }
  
  gfx->setTextColor(maxTemp > cfg.temp_threshold_high ? COLOR_WARN : COLOR_LINE);
  gfx->setCursor(90, 300);
  gfx->printf("%.0fC  Fan:%d%%  PSU:%.1fV", maxTemp, fanSpeed, psuVoltage);
}

void drawGraphMode() {
  gfx->fillScreen(COLOR_BG);
  
  // Header
  gfx->fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(100, 6);
  gfx->print("TEMPERATURE HISTORY");
  
  char timeLabel[40];
  if (cfg.graph_timespan_seconds >= 60) {
    sprintf(timeLabel, " - %d minutes", cfg.graph_timespan_seconds / 60);
  } else {
    sprintf(timeLabel, " - %d seconds", cfg.graph_timespan_seconds);
  }
  gfx->setTextSize(1);
  gfx->setCursor(330, 10);
  gfx->print(timeLabel);
  
  gfx->drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  
  // Full screen graph
  drawTempGraph(20, 40, 440, 270);
}

void updateGraphMode() {
  // Redraw graph
  drawTempGraph(20, 40, 440, 270);
}

void drawNetworkMode() {
  // Header
  gfx->fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(120, 6);
  gfx->print("NETWORK STATUS");

  gfx->drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);

  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_TEXT);

  if (inAPMode) {
    // AP Mode display
    gfx->setCursor(60, 50);
    gfx->setTextColor(COLOR_WARN);
    gfx->print("WiFi Config Mode Active");

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(10, 90);
    gfx->print("1. Connect to WiFi network:");

    gfx->setTextSize(2);
    gfx->setTextColor(COLOR_VALUE);
    gfx->setCursor(40, 110);
    gfx->print("CNC-Guardian-Setup");

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(10, 145);
    gfx->print("2. Open browser and go to:");

    gfx->setTextSize(2);
    gfx->setTextColor(COLOR_VALUE);
    gfx->setCursor(80, 165);
    gfx->print("http://192.168.4.1");

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(10, 200);
    gfx->print("3. Configure your WiFi settings");

    gfx->setCursor(10, 230);
    gfx->setTextColor(COLOR_LINE);
    gfx->print("Temperature monitoring continues in background");

    // Show to exit AP mode
    gfx->setTextColor(COLOR_ORANGE);
    gfx->setCursor(10, 270);
    gfx->print("Press button briefly to return to monitoring");

  } else {
    // Normal network status display
    if (WiFi.status() == WL_CONNECTED) {
      gfx->setCursor(130, 50);
      gfx->setTextColor(COLOR_GOOD);
      gfx->print("WiFi Connected");

      gfx->setTextSize(1);
      gfx->setTextColor(COLOR_TEXT);

      gfx->setCursor(10, 90);
      gfx->print("SSID:");
      gfx->setTextColor(COLOR_VALUE);
      gfx->setCursor(80, 90);
      gfx->print(WiFi.SSID());

      gfx->setTextColor(COLOR_TEXT);
      gfx->setCursor(10, 115);
      gfx->print("IP Address:");
      gfx->setTextColor(COLOR_VALUE);
      gfx->setCursor(80, 115);
      gfx->print(WiFi.localIP());

      gfx->setTextColor(COLOR_TEXT);
      gfx->setCursor(10, 140);
      gfx->print("Signal:");
      gfx->setTextColor(COLOR_VALUE);
      gfx->setCursor(80, 140);
      int rssi = WiFi.RSSI();
      gfx->printf("%d dBm", rssi);

      gfx->setTextColor(COLOR_TEXT);
      gfx->setCursor(10, 165);
      gfx->print("mDNS:");
      gfx->setTextColor(COLOR_VALUE);
      gfx->setCursor(80, 165);
      gfx->printf("http://%s.local", cfg.device_name);

      if (fluidncConnected) {
        gfx->setTextColor(COLOR_TEXT);
        gfx->setCursor(10, 190);
        gfx->print("FluidNC:");
        gfx->setTextColor(COLOR_GOOD);
        gfx->setCursor(80, 190);
        gfx->print("Connected");
      } else {
        gfx->setTextColor(COLOR_TEXT);
        gfx->setCursor(10, 190);
        gfx->print("FluidNC:");
        gfx->setTextColor(COLOR_WARN);
        gfx->setCursor(80, 190);
        gfx->print("Disconnected");
      }

    } else {
      gfx->setCursor(120, 50);
      gfx->setTextColor(COLOR_WARN);
      gfx->print("WiFi Not Connected");

      gfx->setTextSize(1);
      gfx->setTextColor(COLOR_TEXT);
      gfx->setCursor(10, 100);
      gfx->print("Temperature monitoring active (standalone mode)");

      gfx->setCursor(10, 130);
      gfx->setTextColor(COLOR_ORANGE);
      gfx->print("To configure WiFi:");
    }

    // Instructions for entering AP mode
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_LINE);
    gfx->setCursor(10, 250);
    gfx->print("Hold button for 10 seconds to enter WiFi");
    gfx->setCursor(10, 265);
    gfx->print("configuration mode");
  }
}

void updateNetworkMode() {
  // Update time in header if needed - but network info is mostly static
  // Could add dynamic signal strength updates here
}

void drawTempGraph(int x, int y, int w, int h) {
  gfx->fillRect(x, y, w, h, COLOR_BG);
  gfx->drawRect(x, y, w, h, COLOR_LINE);
  
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
    
    gfx->drawLine(x1, y1, x2, y2, color);
  }
  
  // Scale markers
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_LINE);
  gfx->setCursor(x + 3, y + 2);
  gfx->print("60");
  gfx->setCursor(x + 3, y + h / 2 - 5);
  gfx->print("35");
  gfx->setCursor(x + 3, y + h - 10);
  gfx->print("10");
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
    
    if (pressDuration >= 10000) {
      enterSetupMode();
    } else if (pressDuration < 1000) {
      cycleDisplayMode();
    }
  }
  else if (buttonPressed && (millis() - buttonPressStart >= 3000)) {
    showHoldProgress();
  }
}

void cycleDisplayMode() {
  currentMode = (DisplayMode)((currentMode + 1) % 4);  // Now we have 4 modes
  drawScreen();
  
  // Flash mode name
  gfx->fillRect(180, 140, 120, 40, COLOR_HEADER);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(190, 150);
  
  switch(currentMode) {
    case MODE_MONITOR: gfx->print("MONITOR"); break;
    case MODE_ALIGNMENT: gfx->print("ALIGNMENT"); break;
    case MODE_GRAPH: gfx->print("GRAPH"); break;
  }
  
  delay(800);
  drawScreen();
}

void showHoldProgress() {
  unsigned long elapsed = millis() - buttonPressStart;
  int progress = map(elapsed, 3000, 10000, 0, 100);
  progress = constrain(progress, 0, 100);
  
  gfx->fillRect(140, 280, 200, 30, COLOR_BG);
  gfx->drawRect(140, 280, 200, 30, COLOR_TEXT);
  gfx->setTextColor(COLOR_WARN);
  gfx->setTextSize(1);
  gfx->setCursor(145, 285);
  gfx->print("Hold for Setup...");
  
  int barWidth = map(progress, 0, 100, 0, 190);
  gfx->fillRect(145, 295, barWidth, 10, COLOR_WARN);
  
  gfx->setCursor(145, 307);
  gfx->print(10 - (elapsed / 1000));
  gfx->print(" sec");
}

void enterSetupMode() {
  Serial.println("Entering WiFi configuration AP mode...");

  // Stop any existing WiFi connection
  WiFi.disconnect();
  delay(100);

  // Start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CNC-Guardian-Setup");
  inAPMode = true;

  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());

  // Show AP mode screen
  currentMode = MODE_NETWORK;
  drawScreen();

  Serial.println("WiFi configuration AP active. Device will continue monitoring.");
}

void showSplashScreen() {
  gfx->setTextColor(COLOR_HEADER);
  gfx->setTextSize(3);
  gfx->setCursor(80, 120);
  gfx->println("CNC GUARDIAN");
  gfx->setTextSize(2);
  gfx->setCursor(140, 160);
  gfx->println("v0.7");
  gfx->setCursor(160, 190);
  gfx->println("Initializing...");
}

const char* getMonthName(int month) {
  const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return months[month];
}

// ========== Watchdog Timer Functions ==========
// Note: enableLoopWDT() and feedLoopWDT() are provided by the ESP32 Arduino framework
