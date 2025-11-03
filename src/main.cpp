/*
 * FluidDash v0.1 - CYD Edition with JSON Screen Layouts
 * Configured for ESP32-2432S028 (CYD 3.5" or 4.0" modules)
 * - WiFiManager for initial setup
 * - Preferences for persistent storage
 * - Web interface for all settings
 * - Configurable graph timespan
 */

#include <Arduino.h>
#include "config/pins.h"
#include "config/config.h"
#include "display/display.h"
#include "display/screen_renderer.h"
#include "display/ui_modes.h"
#include "sensors/sensors.h"
#include "network/network.h"
#include "utils/utils.h"
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

// Display instance is now in display.cpp (extern declaration in display.h)
RTC_DS3231 rtc;
WebSocketsClient webSocket;
Preferences prefs;  // Needed for WiFi credentials storage
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

// ========== Function Prototypes ==========
void setupWebServer();
String getMainHTML();
String getSettingsHTML();
String getAdminHTML();
String getWiFiConfigHTML();
String getConfigJSON();
String getStatusJSON();
// Display module functions are now in display/ui_modes.h and display/screen_renderer.h
// Sensor functions are now in sensors/sensors.h
// Network functions are now in network/network.h
// Utility functions are now in utils/utils.h

void IRAM_ATTR tachISR() {
  tachCounter++;
}

// JSON parsing, screen rendering, and display modes are now in display module

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

// allocateHistoryBuffer() is now in utils/utils.cpp

// ========== WiFiManager Setup ==========
// WiFi manager setup is now in network/network.cpp

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
  
// Get JSON file - FIXED VERSION (prevents crash)
server.on("/get-json", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->hasParam("file")){
        request->send(400, "application/json", "{\"success\":false,\"message\":\"No file\"}");
        return;
    }
    
    String filename = request->getParam("file")->value();
    String path = "/screens/" + filename;
    
    File file = SD.open(path, FILE_READ);
    if(!file){
        request->send(404, "application/json", "{\"success\":false,\"message\":\"File not found\"}");
        return;
    }
    
    // Send file directly without parsing (prevents memory crash)
    request->send(file, path, "application/json");
    file.close();
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
  
// Editor page - FIXED VERSION
server.on("/editor", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><title>JSON Editor</title>";
    html += "<style>body{margin:0;background:#1a1a1a;color:#fff;font-family:monospace}";
    html += ".container{display:flex;height:100vh}.sidebar{width:200px;background:#2a2a2a;padding:10px}";
    html += ".editor{flex:1;display:flex;flex-direction:column;padding:10px}";
    html += "textarea{flex:1;background:#0a0a0a;color:#0f0;border:1px solid #00bfff;padding:10px;font-family:monospace;font-size:12px}";
    html += "button{background:#00bfff;color:#000;padding:10px 20px;border:none;margin:5px;cursor:pointer}";
    html += ".file-btn{background:#2a2a2a;color:#fff;padding:8px;margin:2px 0;width:100%;text-align:left;cursor:pointer}";
    html += ".file-btn:hover{background:#3a3a3a}";
    html += "#status{padding:10px;margin:10px 0}.success{background:#004d00;color:#0f0}";
    html += ".error{background:#4d0000;color:#f00}</style></head><body>";
    html += "<div class='container'><div class='sidebar'><h3>Files</h3>";
    html += "<button class='file-btn' onclick=\"load('monitor.json')\">monitor.json</button>";
    html += "<button class='file-btn' onclick=\"load('alignment.json')\">alignment.json</button>";
    html += "<button class='file-btn' onclick=\"load('graph.json')\">graph.json</button>";
    html += "<button class='file-btn' onclick=\"load('network.json')\">network.json</button></div>";
    html += "<div class='editor'><h2 id='fn'>Select file</h2>";
    html += "<textarea id='ed' placeholder='Load a JSON file...'></textarea><div>";
    html += "<button onclick='save()'>Save</button>";
    html += "<button onclick='check()'>Validate</button>";
    html += "<button onclick='reload()'>Reload</button></div>";
    html += "<div id='st'></div></div></div>";
    html += "<script>let cf='';";
    html += "function msg(m,t){document.getElementById('st').innerHTML=m;document.getElementById('st').className=t}";
    html += "function load(f){cf=f;document.getElementById('fn').innerText='Editing: '+f;";
    html += "fetch('/get-json?file='+f).then(r=>r.text()).then(txt=>{";
    html += "try{let j=JSON.parse(txt);document.getElementById('ed').value=JSON.stringify(j,null,2);";
    html += "msg('Loaded: '+f,'success')}catch(e){msg('Parse error','error')}})";
    html += ".catch(e=>msg('Load failed','error'))}";
    html += "function save(){if(!cf){msg('No file','error');return}";
    html += "let c=document.getElementById('ed').value;try{JSON.parse(c)}catch(e){";
    html += "msg('Invalid JSON','error');return}";
    html += "fetch('/save-json',{method:'POST',headers:{'Content-Type':'application/json'},";
    html += "body:JSON.stringify({filename:cf,content:c})}).then(r=>r.json()).then(d=>{";
    html += "if(d.success)msg('Saved!','success');else msg('Save failed','error')})}";
    html += "function check(){try{let c=document.getElementById('ed').value;";
    html += "let p=JSON.parse(c);let ec=p.elements?p.elements.length:0;";
    html += "msg('Valid JSON ('+ec+' elements)','success')}";
    html += "catch(e){msg('Invalid: '+e.message,'error')}}";
    html += "function reload(){fetch('/api/reload-screens',{method:'POST'}).then(r=>r.json())";
    html += ".then(d=>msg('Layouts reloaded!','success'))";
    html += ".catch(e=>msg('Reload failed','error'))}</script></body></html>";
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
// FluidNC connection functions are now in network/network.cpp

// ========== Core Functions ==========
// Sensor functions are now in sensors/sensors.cpp
// Display functions are now in display/ui_modes.cpp

// ========== Watchdog Functions ==========
// (Watchdog function implementations would go here if needed)
