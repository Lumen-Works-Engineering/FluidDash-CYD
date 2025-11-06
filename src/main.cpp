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
#include <WebSocketsClient.h>

#include <Preferences.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
// #include "upload_queue.h"  // DISABLED: Phase 1 - will re-enable in Phase 2 with SPIFFS
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
// bool processQueuedUpload();  // DISABLED: Phase 1 - will re-enable in Phase 2 with SPIFFS
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

// ============ WEB SERVER HTML TEMPLATES (PROGMEM) ============

const char MAIN_HTML[] PROGMEM = R"rawliteral(
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
      <p><strong>Device Name:</strong> %DEVICE_NAME%.local</p>
      <p><strong>IP Address:</strong> %IP_ADDRESS%</p>
      <p><strong>FluidNC:</strong> %FLUIDNC_IP%</p>
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
)rawliteral";

const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
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
        <input type='number' name='temp_low' value='%TEMP_LOW%' step='0.5' min='20' max='50'>

        <label>High Threshold (°C) - Fan at 100%</label>
        <input type='number' name='temp_high' value='%TEMP_HIGH%' step='0.5' min='30' max='80'>
      </div>

      <div class='card'>
        <h2>Fan Control</h2>
        <label>Minimum Fan Speed (%)</label>
        <input type='number' name='fan_min' value='%FAN_MIN%' min='0' max='100'>
      </div>

      <div class='card'>
        <h2>PSU Monitoring</h2>
        <label>Low Voltage Alert (V)</label>
        <input type='number' name='psu_low' value='%PSU_LOW%' step='0.1' min='18' max='24'>

        <label>High Voltage Alert (V)</label>
        <input type='number' name='psu_high' value='%PSU_HIGH%' step='0.1' min='24' max='30'>
      </div>

      <div class='card'>
        <h2>Temperature Graph</h2>
        <label>Graph Timespan (seconds)</label>
        <select name='graph_time'>
          <option value='60' %GRAPH_TIME_60%>1 minute</option>
          <option value='300' %GRAPH_TIME_300%>5 minutes</option>
          <option value='600' %GRAPH_TIME_600%>10 minutes</option>
          <option value='1800' %GRAPH_TIME_1800%>30 minutes</option>
          <option value='3600' %GRAPH_TIME_3600%>60 minutes</option>
        </select>

        <label>Update Interval (seconds)</label>
        <select name='graph_interval'>
          <option value='1' %GRAPH_INT_1%>1 second</option>
          <option value='5' %GRAPH_INT_5%>5 seconds</option>
          <option value='10' %GRAPH_INT_10%>10 seconds</option>
          <option value='30' %GRAPH_INT_30%>30 seconds</option>
          <option value='60' %GRAPH_INT_60%>60 seconds</option>
        </select>
      </div>

      <div class='card'>
        <h2>Display</h2>
        <label>Coordinate Decimal Places</label>
        <select name='coord_decimals'>
          <option value='2' %COORD_DEC_2%>2 decimals (0.00)</option>
          <option value='3' %COORD_DEC_3%>3 decimals (0.000)</option>
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
)rawliteral";

const char ADMIN_HTML[] PROGMEM = R"rawliteral(
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
        <input type='number' name='cal_x' value='%CAL_X%' step='0.1'>

        <label>YL-Axis Offset (°C)</label>
        <input type='number' name='cal_yl' value='%CAL_YL%' step='0.1'>

        <label>YR-Axis Offset (°C)</label>
        <input type='number' name='cal_yr' value='%CAL_YR%' step='0.1'>

        <label>Z-Axis Offset (°C)</label>
        <input type='number' name='cal_z' value='%CAL_Z%' step='0.1'>
      </div>

      <div class='card'>
        <h2>PSU Voltage Calibration</h2>
        <p style='color:#aaa'>Voltage divider multiplier (measure with multimeter)</p>

        <label>Calibration Factor</label>
        <input type='number' name='psu_cal' value='%PSU_CAL%' step='0.01' min='5' max='10'>
      </div>

      <div class='success' id='success'>Calibration saved successfully!</div>

      <button type='submit'>💾 Save Calibration</button>
      <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
    </form>

    <div class='card' style='margin-top:30px;border-top:3px solid #ff6600'>
      <h2>🕒 Real-Time Clock (RTC)</h2>
      <p style='color:#aaa'>Set the current date and time for the DS3231 RTC module</p>

      <div id='currentTime' class='current-reading' style='margin-bottom:20px'>Loading...</div>

      <form id='rtcForm'>
        <label>Date (YYYY-MM-DD)</label>
        <input type='date' id='rtcDate' required>

        <label>Time (HH:MM:SS)</label>
        <input type='time' id='rtcTime' step='1' required>

        <div class='success' id='rtcSuccess'>RTC time set successfully!</div>

        <button type='submit'>⏰ Set RTC Time</button>
        <button type='button' onclick='setRTCNow()'>📅 Use Browser Time</button>
      </form>
    </div>
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

    function updateRTCTime() {
      fetch('/api/rtc')
        .then(r => r.json())
        .then(data => {
          if (data.timestamp) {
            document.getElementById('currentTime').innerHTML =
              `Current RTC Time: ${data.timestamp}`;
          }
        });
    }

    function setRTCNow() {
      const now = new Date();
      const date = now.toISOString().split('T')[0];
      const time = now.toTimeString().split(' ')[0];
      document.getElementById('rtcDate').value = date;
      document.getElementById('rtcTime').value = time;
    }

    document.getElementById('rtcForm').addEventListener('submit', function(e) {
      e.preventDefault();

      const date = document.getElementById('rtcDate').value;
      const time = document.getElementById('rtcTime').value;

      const formData = new URLSearchParams();
      formData.append('date', date);
      formData.append('time', time);

      fetch('/api/rtc/set', {
        method: 'POST',
        body: formData
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          document.getElementById('rtcSuccess').style.display = 'block';
          setTimeout(() => {
            document.getElementById('rtcSuccess').style.display = 'none';
          }, 3000);
          updateRTCTime();
        }
      });
    });

    updateReadings();
    updateRTCTime();
    setInterval(updateReadings, 2000);
    setInterval(updateRTCTime, 5000);
  </script>
</body>
</html>
)rawliteral";

const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
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

%WIFI_STATUS%

    <div class='info-box'>
      <strong>ℹ️ Manual WiFi Configuration</strong><br>
      Enter your WiFi network name (SSID) and password below. The device will restart and attempt to connect.
    </div>

    <form id='wifiForm'>
      <div class='card'>
        <h2>WiFi Credentials</h2>

        <label>Network Name (SSID)</label>
        <input type='text' id='ssid' name='ssid' value='%CURRENT_SSID%' required
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
)rawliteral";

// ============ WEB SERVER FUNCTIONS ============

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

// ========== Configuration Management ==========

// allocateHistoryBuffer() is now in utils/utils.cpp

// ========== WiFiManager Setup ==========
// WiFi manager setup is now in network/network.cpp

// ========== Web Server Setup ==========

// Web server handler functions
void handleRoot() {
  server.send(200, "text/html", getMainHTML());
}

void handleSettings() {
  server.send(200, "text/html", getSettingsHTML());
}

void handleAdmin() {
  server.send(200, "text/html", getAdminHTML());
}

void handleWiFi() {
  server.send(200, "text/html", getWiFiConfigHTML());
}

void handleAPIConfig() {
  server.send(200, "application/json", getConfigJSON());
}

void handleAPIStatus() {
  server.send(200, "application/json", getStatusJSON());
}

void handleAPISave() {
  // Update config from POST parameters
  if (server.hasArg("temp_low")) {
    cfg.temp_threshold_low = server.arg("temp_low").toFloat();
  }
  if (server.hasArg("temp_high")) {
    cfg.temp_threshold_high = server.arg("temp_high").toFloat();
  }
  if (server.hasArg("fan_min")) {
    cfg.fan_min_speed = server.arg("fan_min").toInt();
  }
  if (server.hasArg("graph_time")) {
    uint16_t newTime = server.arg("graph_time").toInt();
    if (newTime != cfg.graph_timespan_seconds) {
      cfg.graph_timespan_seconds = newTime;
      allocateHistoryBuffer(); // Reallocate with new size
    }
  }
  if (server.hasArg("graph_interval")) {
    cfg.graph_update_interval = server.arg("graph_interval").toInt();
  }
  if (server.hasArg("psu_low")) {
    cfg.psu_alert_low = server.arg("psu_low").toFloat();
  }
  if (server.hasArg("psu_high")) {
    cfg.psu_alert_high = server.arg("psu_high").toFloat();
  }
  if (server.hasArg("coord_decimals")) {
    cfg.coord_decimal_places = server.arg("coord_decimals").toInt();
  }

  saveConfig();
  server.send(200, "text/plain", "Settings saved successfully");
}

void handleAPIAdminSave() {
  if (server.hasArg("cal_x")) {
    cfg.temp_offset_x = server.arg("cal_x").toFloat();
  }
  if (server.hasArg("cal_yl")) {
    cfg.temp_offset_yl = server.arg("cal_yl").toFloat();
  }
  if (server.hasArg("cal_yr")) {
    cfg.temp_offset_yr = server.arg("cal_yr").toFloat();
  }
  if (server.hasArg("cal_z")) {
    cfg.temp_offset_z = server.arg("cal_z").toFloat();
  }
  if (server.hasArg("psu_cal")) {
    cfg.psu_voltage_cal = server.arg("psu_cal").toFloat();
  }

  saveConfig();
  server.send(200, "text/plain", "Calibration saved successfully");
}

void handleAPIResetWiFi() {
  wm.resetSettings();
  server.send(200, "text/plain", "WiFi settings cleared. Device will restart...");
  delay(1000);
  ESP.restart();
}

void handleAPIRestart() {
  server.send(200, "text/plain", "Restarting device...");
  delay(1000);
  ESP.restart();
}

void handleAPIWiFiConnect() {
  String ssid = "";
  String password = "";

  if (server.hasArg("ssid")) {
    ssid = server.arg("ssid");
  }
  if (server.hasArg("password")) {
    password = server.arg("password");
  }

  if (ssid.length() == 0) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
    return;
  }

  Serial.println("Attempting to connect to: " + ssid);

  // Store credentials in preferences
  prefs.begin("fluiddash", false);
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", password);
  prefs.end();

  // Send response and restart to apply credentials
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved. Device will restart and attempt to connect.\"}");

  Serial.println("WiFi credentials saved. Restarting...");
  delay(2000);
  ESP.restart();
}

void handleAPIReloadScreens() {
    // PHASE 2 FINAL: Reboot-based workflow (avoids mutex/context issues)
    Serial.println("[API] Layout reload requested - rebooting device");

    server.send(200, "application/json",
        "{\"status\":\"Rebooting device to load new layouts...\",\"message\":\"Device will restart in 1 second\"}");

    delay(1000);  // Let response send
    ESP.restart();
}

void handleAPIRTC() {
    // Get current time from RTC
    DateTime now = rtc.now();

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    String json = "{\"success\":true,\"timestamp\":\"";
    json += timestamp;
    json += "\"}";

    server.send(200, "application/json", json);
}

void handleAPIRTCSet() {
    // Parse date and time from POST parameters
    if (!server.hasArg("date") || !server.hasArg("time")) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing date or time parameter\"}");
        return;
    }

    String dateStr = server.arg("date");  // Format: YYYY-MM-DD
    String timeStr = server.arg("time");  // Format: HH:MM:SS

    // Parse date: YYYY-MM-DD
    int year = dateStr.substring(0, 4).toInt();
    int month = dateStr.substring(5, 7).toInt();
    int day = dateStr.substring(8, 10).toInt();

    // Parse time: HH:MM:SS
    int hour = timeStr.substring(0, 2).toInt();
    int minute = timeStr.substring(3, 5).toInt();
    int second = timeStr.substring(6, 8).toInt();

    // Validate ranges
    if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > 31 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Invalid date/time values\"}");
        return;
    }

    // Set the RTC time
    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    Serial.printf("[RTC] Time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);

    server.send(200, "application/json",
        "{\"success\":true,\"message\":\"RTC time updated successfully\"}");
}

// ========== PHASE 2: SPIFFS-BASED UPLOAD SYSTEM ==========

void handleUpload() {
  String html = "<!DOCTYPE html><html><head><title>Upload JSON</title>";
  html += "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff}";
  html += "h1{color:#00bfff}.box{background:#2a2a2a;padding:20px;border-radius:8px;max-width:600px}";
  html += "button{background:#00bfff;color:#000;padding:10px 20px;border:none;cursor:pointer;margin:5px}";
  html += "#status{margin-top:20px;padding:10px}.success{background:#004d00;color:#0f0}";
  html += ".error{background:#4d0000;color:#f00}.info{background:#003d5c;color:#00bfff}";
  html += ".note{background:#2a2a2a;padding:10px;margin:10px 0;border-left:3px solid #00bfff}</style></head><body>";
  html += "<h1>Upload JSON Layout</h1><div class='box'>";
  html += "<div class='note'><strong>Note:</strong> After upload, device must reboot to load new layouts.</div>";
  html += "<h3>Upload Screen Layout</h3>";
  html += "<form id='f' enctype='multipart/form-data'>";
  html += "<input type='file' id='file' accept='.json' required><br><br>";
  html += "<button type='submit'>Upload to SPIFFS</button></form>";
  html += "<div id='status'></div>";
  html += "<div id='reboot' style='display:none;margin-top:10px'>";
  html += "<button onclick='reboot()'>Reboot Device Now</button></div></div>";
  html += "<script>function reboot(){document.getElementById('status').innerHTML='Rebooting...';";
  html += "document.getElementById('status').className='info';fetch('/api/reboot').then(()=>{";
  html += "setTimeout(()=>{window.location.href='/'},3000)});}";
  html += "document.getElementById('f').addEventListener('submit',function(e){";
  html += "e.preventDefault();let file=document.getElementById('file').files[0];";
  html += "if(!file)return;let s=document.getElementById('status');";
  html += "s.innerHTML='Uploading to SPIFFS...';s.className='info';let fd=new FormData();";
  html += "fd.append('file',file);fetch('/upload-json',{method:'POST',body:fd})";
  html += ".then(r=>r.json()).then(d=>{if(d.success){";
  html += "s.innerHTML='Upload successful! File saved to SPIFFS. Click button to reboot and load new layouts.';";
  html += "s.className='success';document.getElementById('reboot').style.display='block'}";
  html += "else{s.innerHTML='Upload failed';s.className='error'}})";
  html += ".catch(e=>{s.innerHTML='Upload failed';s.className='error'})});</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Static variables shared between upload handler and completion handler
static String uploadData;
static String uploadFilename;
static bool uploadError = false;

void handleUploadJSON() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        uploadError = false;
        uploadData = "";
        uploadFilename = upload.filename;

        // Validate filename
        if (!uploadFilename.endsWith(".json")) {
            Serial.println("[Upload] Not a JSON file");
            uploadError = true;
            return;
        }

        Serial.printf("[Upload] Starting: %s\n", uploadFilename.c_str());

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!uploadError && upload.currentSize) {
            // Accumulate data in memory
            if (uploadData.length() + upload.currentSize > 8192) {
                Serial.println("[Upload] File too large (max 8KB)");
                uploadError = true;
                return;
            }
            // Append chunk to uploadData
            for (size_t i = 0; i < upload.currentSize; i++) {
                uploadData += (char)upload.buf[i];
            }
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (!uploadError) {
            // PHASE 2: Write directly to SPIFFS (safe - no SD mutex issues)
            String filepath = "/screens/" + uploadFilename;

            Serial.printf("[Upload] Saving %d bytes to SPIFFS: %s\n",
                         uploadData.length(), filepath.c_str());

            if (storage.saveFile(filepath.c_str(), uploadData)) {
                Serial.println("[Upload] SUCCESS: Saved to SPIFFS");
                uploadError = false;
            } else {
                Serial.println("[Upload] ERROR: SPIFFS write failed");
                uploadError = true;
            }
        }
        // Clear accumulated data
        uploadData = "";
        uploadFilename = "";
    }
}

void handleUploadComplete() {
    // This is called after the upload finishes - send response to client
    if (uploadError) {
        server.send(500, "application/json", "{\"success\":false,\"message\":\"Upload failed\"}");
    } else {
        server.send(200, "application/json",
            "{\"success\":true,\"message\":\"Uploaded to SPIFFS successfully\"}");
    }
}

void handleUploadStatus() {
    // Return storage status (SPIFFS-based system)
    JsonDocument doc;
    doc["spiffsAvailable"] = storage.isSPIFFSAvailable();
    doc["sdAvailable"] = storage.isSDAvailable();
    doc["message"] = "Upload saves to SPIFFS, auto-loads on reload";

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ========== END PHASE 2 UPLOAD SYSTEM ==========

void handleGetJSON() {
  // DISABLED - was causing crashes with SD card access
  server.send(503, "application/json", "{\"success\":false,\"message\":\"Endpoint disabled - causing crashes\"}");
}

void handleSaveJSON() {
  if (!server.hasArg("filename") || !server.hasArg("content")) {
    server.send(400, "text/plain", "Missing filename or content");
    return;
  }

  String filename = server.arg("filename");
  String content = server.arg("content");

  File file = SD.open(filename, FILE_WRITE);
  yield();
  if (!file) {
    server.send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  file.print(content);
  yield();
  file.close();
  yield();

  server.send(200, "text/plain", "File saved successfully");
}

void handleEditor() {
  String html = "<!DOCTYPE html><html><head><title>Editor Disabled</title>";
  html += "<style>body{font-family:Arial;margin:50px;background:#1a1a1a;color:#fff;text-align:center}";
  html += "h1{color:#ff6600}a{color:#00bfff;text-decoration:none}</style></head><body>";
  html += "<h1>JSON Editor Temporarily Disabled</h1>";
  html += "<p>The live editor was causing memory issues and crashes.</p>";
  html += "<p>Please use the <a href='/upload'>Upload Page</a> to upload JSON files instead.</p>";
  html += "<p><a href='/'>Back to Dashboard</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setupWebServer() {
  // Register all handlers
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/wifi", HTTP_GET, handleWiFi);
  server.on("/api/config", HTTP_GET, handleAPIConfig);
  server.on("/api/status", HTTP_GET, handleAPIStatus);
  server.on("/api/save", HTTP_POST, handleAPISave);
  server.on("/api/admin/save", HTTP_POST, handleAPIAdminSave);
  server.on("/api/reset-wifi", HTTP_POST, handleAPIResetWiFi);
  server.on("/api/restart", HTTP_POST, handleAPIRestart);
  server.on("/api/reboot", HTTP_GET, []() {
      server.send(200, "application/json",
          "{\"status\":\"Rebooting device...\",\"message\":\"Device will restart in 1 second\"}");
      delay(1000);  // Let response send
      ESP.restart();
  });
  server.on("/api/wifi/connect", HTTP_POST, handleAPIWiFiConnect);
  server.on("/api/reload-screens", HTTP_POST, handleAPIReloadScreens);
  server.on("/api/reload-screens", HTTP_GET, handleAPIReloadScreens);  // Also accept GET
  server.on("/api/rtc", HTTP_GET, handleAPIRTC);
  server.on("/api/rtc/set", HTTP_POST, handleAPIRTCSet);
  // PHASE 2: Re-enabled with SPIFFS-based upload (safe)
  server.on("/api/upload-status", HTTP_GET, handleUploadStatus);
  server.on("/upload", HTTP_GET, handleUpload);
  server.on("/upload-json", HTTP_POST, handleUploadComplete, handleUploadJSON);
  server.on("/get-json", HTTP_GET, handleGetJSON);
  server.on("/save-json", HTTP_POST, handleSaveJSON);
  server.on("/editor", HTTP_GET, handleEditor);

  server.begin();
  Serial.println("Web server started");
}

// ========== HTML Pages ==========
// NOTE: These functions return large String objects which can cause heap fragmentation.
// For production use, consider using AsyncWebServerResponse streaming or PROGMEM storage.
// Current implementation is acceptable for occasional web interface access.

String getMainHTML() {
  String html = String(FPSTR(MAIN_HTML));

  // Replace all placeholders with dynamic content
  html.replace("%DEVICE_NAME%", cfg.device_name);
  html.replace("%IP_ADDRESS%", WiFi.localIP().toString());
  html.replace("%FLUIDNC_IP%", cfg.fluidnc_ip);

  return html;
}

String getSettingsHTML() {
  String html = String(FPSTR(SETTINGS_HTML));

  // Replace numeric input values
  html.replace("%TEMP_LOW%", String(cfg.temp_threshold_low));
  html.replace("%TEMP_HIGH%", String(cfg.temp_threshold_high));
  html.replace("%FAN_MIN%", String(cfg.fan_min_speed));
  html.replace("%PSU_LOW%", String(cfg.psu_alert_low));
  html.replace("%PSU_HIGH%", String(cfg.psu_alert_high));

  // Replace graph timespan selected options
  html.replace("%GRAPH_TIME_60%", cfg.graph_timespan_seconds == 60 ? "selected" : "");
  html.replace("%GRAPH_TIME_300%", cfg.graph_timespan_seconds == 300 ? "selected" : "");
  html.replace("%GRAPH_TIME_600%", cfg.graph_timespan_seconds == 600 ? "selected" : "");
  html.replace("%GRAPH_TIME_1800%", cfg.graph_timespan_seconds == 1800 ? "selected" : "");
  html.replace("%GRAPH_TIME_3600%", cfg.graph_timespan_seconds == 3600 ? "selected" : "");

  // Replace graph interval selected options
  html.replace("%GRAPH_INT_1%", cfg.graph_update_interval == 1 ? "selected" : "");
  html.replace("%GRAPH_INT_5%", cfg.graph_update_interval == 5 ? "selected" : "");
  html.replace("%GRAPH_INT_10%", cfg.graph_update_interval == 10 ? "selected" : "");
  html.replace("%GRAPH_INT_30%", cfg.graph_update_interval == 30 ? "selected" : "");
  html.replace("%GRAPH_INT_60%", cfg.graph_update_interval == 60 ? "selected" : "");

  // Replace coordinate decimal places selected options
  html.replace("%COORD_DEC_2%", cfg.coord_decimal_places == 2 ? "selected" : "");
  html.replace("%COORD_DEC_3%", cfg.coord_decimal_places == 3 ? "selected" : "");

  return html;
}

String getAdminHTML() {
  String html = String(FPSTR(ADMIN_HTML));

  // Replace calibration offset values (with 2 decimal places for temp)
  html.replace("%CAL_X%", String(cfg.temp_offset_x, 2));
  html.replace("%CAL_YL%", String(cfg.temp_offset_yl, 2));
  html.replace("%CAL_YR%", String(cfg.temp_offset_yr, 2));
  html.replace("%CAL_Z%", String(cfg.temp_offset_z, 2));

  // Replace PSU calibration value (with 3 decimal places)
  html.replace("%PSU_CAL%", String(cfg.psu_voltage_cal, 3));

  return html;
}

String getWiFiConfigHTML() {
  String html = String(FPSTR(WIFI_CONFIG_HTML));

  // Get current WiFi status
  String currentSSID = WiFi.SSID();
  String currentIP = WiFi.localIP().toString();
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  bool isAPMode = inAPMode;

  // Build WiFi status section
  String wifiStatus = "<div class='status ";
  if (isAPMode) {
    wifiStatus += "status-ap'>🔧 AP Mode Active - Configure WiFi to connect to your network</div>";
  } else if (isConnected) {
    wifiStatus += "status-connected'>✅ Connected to: " + currentSSID + "<br>IP: " + currentIP + "</div>";
  } else {
    wifiStatus += "status-disconnected'>❌ Not Connected - Configure WiFi below</div>";
  }

  // Replace placeholders
  html.replace("%WIFI_STATUS%", wifiStatus);
  html.replace("%CURRENT_SSID%", currentSSID);

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
