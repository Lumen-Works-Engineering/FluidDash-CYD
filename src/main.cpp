/*
 * FluidDash v0.9.1 - CYD Edition with JSON Screen Layouts
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
#include <WebSocketsClient.h>
#include <Preferences.h>
#include "web/web_server.h"
#include "web/web_handlers.h"
#include "web/web_api.h" 
#include "web/html_pages.h"
#include "web/html_templates.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "storage_manager.h"

// Storage manager - handles SD/SPIFFS with fallback
StorageManager storage;

// Upload queue - handlers queue uploads, loop() executes them
// SDUploadQueue uploadQueue;  // DISABLED: Phase 1 - will re-enable in Phase 2 with SPIFFS


// Display instance is now in display.cpp (extern declaration in display.h)
RTC_DS3231 rtc;
WebSocketsClient webSocket;
Preferences prefs;  // Needed for WiFi credentials storage
WebServer server(80);
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
String getConfigJSON();
String getStatusJSON();

void IRAM_ATTR tachISR() {
  tachCounter++;
}

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

  // Initialize DS18B20 temperature sensors
  feedLoopWDT();
  initDS18B20Sensors();

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
    // ========== INITIALIZE STORAGE MANAGER ==========
    feedLoopWDT();
    Serial.println("\n=== Initializing Storage Manager ===");

    // Initialize SD card on VSPI bus (needed before StorageManager)
    SPIClass spiSD(VSPI);
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    SD.begin(SD_CS, spiSD);  // Attempt SD init (no error if fails)

    // Initialize StorageManager (handles SD + SPIFFS with fallback)
    if (storage.begin()) {
        Serial.println("SUCCESS: Storage Manager initialized!");
        sdCardAvailable = storage.isSDAvailable();

        if (sdCardAvailable) {
            Serial.println("  - SD card available");
            uint8_t cardType = SD.cardType();
            Serial.print("  - SD Card Type: ");
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
            Serial.printf("  - SD Card Size: %lluMB\n", cardSize);
        } else {
            Serial.println("  - SD card not available, using SPIFFS");
        }

        if (storage.isSPIFFSAvailable()) {
            Serial.println("  - SPIFFS available");
        }
    } else {
        Serial.println("ERROR: Storage Manager initialization failed!");
        sdCardAvailable = false;
    }
    Serial.println("=== Storage Manager Ready ===\n");
    // ========== END STORAGE MANAGER INIT ==========
    // ========== PHASE 2: LOAD JSON SCREEN LAYOUTS ==========
    feedLoopWDT();
    initDefaultLayouts();  // Initialize fallback state

    if (storage.isSPIFFSAvailable() || sdCardAvailable) {
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
        Serial.println("[JSON] No storage available (SD/SPIFFS), using legacy drawing\n");
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
  // Handle web server requests
  server.handleClient();

  // Process one queued upload per loop iteration (safe SD access)
  // This prevents blocking and spreads processing over time
  // DISABLED: Phase 1 - will re-enable in Phase 2 with SPIFFS
  // if (uploadQueue.hasPending()) {
  //   processQueuedUpload();
  //   yield();  // Feed watchdog during upload processing
  // }

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
      yield();  // Yield before WebSocket operations
      webSocket.loop();
      yield();  // Yield after WebSocket operations

      // Always poll for status - FluidNC doesn't have automatic reporting
      if (fluidncConnected && (millis() - lastStatusRequest >= cfg.status_update_rate)) {
          if (debugWebSocket) {
              Serial.println("[FluidNC] Sending status request");
          }
          yield();  // Yield before send
          webSocket.sendTXT("?");
          yield();  // Yield after send
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
