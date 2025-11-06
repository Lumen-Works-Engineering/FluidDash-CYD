# FluidDash-CYD Integration Guide

## 🎯 Purpose

This guide provides step-by-step instructions to integrate the new modular architecture into your existing main.cpp file.

**IMPORTANT**: Make a backup of your main.cpp before proceeding!

```bash
cp src/main.cpp src/main.cpp.backup
```

---

## 📋 Integration Checklist

### Phase 1: Add New Includes ✅

**Location**: Top of main.cpp (after existing includes)

**Add these lines:**

```cpp
// ========== NEW: State Management ==========
#include "state/system_state.h"
#include "state/fluidnc_state.h"

// ========== NEW: Web Server Modules ==========
#include "web/web_server.h"
#include "web/web_handlers.h"
#include "web/web_api.h"
#include "web/html_pages.h"
```

---

### Phase 2: Remove Global Variable Declarations ❌

**Location**: Lines ~40-120 in main.cpp

**DELETE these variable declarations** (they're now in state classes):

```cpp
// ❌ DELETE: Runtime variables
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

// ❌ DELETE: Non-blocking ADC sampling
uint32_t adcSamples[5][10];
uint8_t adcSampleIndex = 0;
uint8_t adcCurrentSensor = 0;
unsigned long lastAdcSample = 0;
bool adcReady = false;

// ❌ DELETE: Dynamic history buffer
float *tempHistory = nullptr;
uint16_t historySize = 0;
uint16_t historyIndex = 0;

// ❌ DELETE: FluidNC status
String machineState = "OFFLINE";
float posX = 0, posY = 0, posZ = 0, posA = 0;
float wposX = 0, wposY = 0, wposZ = 0, wposA = 0;
int feedRate = 0;
int spindleRPM = 0;
bool fluidncConnected = false;
unsigned long jobStartTime = 0;
bool isJobRunning = false;

// ❌ DELETE: Extended status fields
int feedOverride = 100;
int rapidOverride = 100;
int spindleOverride = 100;
float wcoX = 0, wcoY = 0, wcoZ = 0, wcoA = 0;

// ❌ DELETE: WebSocket reporting
bool autoReportingEnabled = false;
unsigned long reportingSetupTime = 0;
bool debugWebSocket = false;

// ❌ DELETE: WiFi AP mode flag
bool inAPMode = false;

// ❌ DELETE: RTC availability flag
bool rtcAvailable = false;

// ❌ DELETE: Timing
unsigned long lastTachRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastHistoryUpdate = 0;
unsigned long lastStatusRequest = 0;
unsigned long sessionStartTime = 0;
unsigned long buttonPressStart = 0;
bool buttonPressed = false;
```

**KEEP these** (not moved to state classes):
```cpp
// ✅ KEEP: Hardware objects
RTC_DS3231 rtc;
WebSocketsClient webSocket;
Preferences prefs;
WebServer server(80);
WiFiManager wm;
StorageManager storage;
```

---

### Phase 3: Update Function Prototypes ✏️

**Location**: Lines ~130-140 in main.cpp

**DELETE these prototypes** (now in web modules):

```cpp
// ❌ DELETE: Function Prototypes
void setupWebServer();
String getMainHTML();
String getSettingsHTML();
String getAdminHTML();
String getWiFiConfigHTML();
String getConfigJSON();
String getStatusJSON();
```

**KEEP**:
```cpp
// ✅ KEEP: ISR
void IRAM_ATTR tachISR() {
  tachCounter++;  // ⚠️ Will need to update to: systemState.tachCounter++;
}
```

---

### Phase 4: Remove HTML PROGMEM Strings ❌

**Location**: Lines ~150-650 in main.cpp

**DELETE these entire PROGMEM blocks:**

```cpp
// ❌ DELETE: const char MAIN_HTML[] PROGMEM = R"rawliteral(...)rawliteral";
// ❌ DELETE: const char SETTINGS_HTML[] PROGMEM = R"rawliteral(...)rawliteral";
// ❌ DELETE: const char ADMIN_HTML[] PROGMEM = R"rawliteral(...)rawliteral";
// ❌ DELETE: const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(...)rawliteral";
```

**Total lines to delete**: ~500 lines of HTML

---

### Phase 5: Initialize State Objects in setup() ➕

**Location**: In `setup()` function, after `initDefaultConfig()` call

**ADD these lines:**

```cpp
void setup() {
    Serial.begin(115200);
    Serial.println("FluidDash - Starting...");

    // Initialize default configuration
    initDefaultConfig();
    
    // ========== NEW: Initialize state management objects ==========
    systemState.init();
    fluidncState.init();
    // ===============================================================

    // Enable watchdog timer (10 seconds)
    enableLoopWDT();
    // ... rest of setup continues ...
}
```

---

### Phase 6: Remove Web Handler Functions ❌

**Location**: Lines ~700-1300 in main.cpp

**DELETE these entire functions:**

```cpp
// ❌ DELETE ALL:
void handleRoot() { ... }
void handleSettings() { ... }
void handleAdmin() { ... }
void handleWiFi() { ... }
void handleAPIConfig() { ... }
void handleAPIStatus() { ... }
void handleAPISave() { ... }
void handleAPIAdminSave() { ... }
void handleAPIResetWiFi() { ... }
void handleAPIRestart() { ... }
void handleAPIWiFiConnect() { ... }
void handleAPIReloadScreens() { ... }
void handleAPIRTC() { ... }
void handleAPIRTCSet() { ... }
void handleUpload() { ... }
void handleUploadJSON() { ... }
void handleUploadComplete() { ... }
void handleUploadStatus() { ... }
void handleGetJSON() { ... }
void handleSaveJSON() { ... }
void handleEditor() { ... }
```

**Total lines to delete**: ~400 lines

---

### Phase 7: Remove setupWebServer() Function ❌

**Location**: Lines ~1400-1450 in main.cpp

**DELETE the entire setupWebServer() function:**

```cpp
// ❌ DELETE:
void setupWebServer() {
    // Register all handlers
    server.on("/", HTTP_GET, handleRoot);
    // ... all the routes ...
    server.begin();
    Serial.println("Web server started");
}
```

**NOTE**: The call to `setupWebServer()` in `setup()` stays - it now calls the function from web_server.cpp

---

### Phase 8: Remove JSON API Functions ❌

**Location**: Lines ~1300-1400 in main.cpp

**DELETE these functions:**

```cpp
// ❌ DELETE:
String getMainHTML() { ... }
String getSettingsHTML() { ... }
String getAdminHTML() { ... }
String getWiFiConfigHTML() { ... }
String getConfigJSON() { ... }
String getStatusJSON() { ... }
```

**Total lines to delete**: ~100 lines

---

### Phase 9: Update Variable References Throughout Code 🔄

This is the most critical step. Use find-and-replace carefully:

#### Temperature Variables

```cpp
// Find: temperatures[
// Replace: systemState.temperatures[

// Find: peakTemps[
// Replace: systemState.peakTemps[

// Find: tempHistory
// Replace: systemState.tempHistory

// Find: historySize
// Replace: systemState.historySize

// Find: historyIndex
// Replace: systemState.historyIndex
```

#### Fan Control

```cpp
// Find: tachCounter
// Replace: systemState.tachCounter

// Find: fanRPM
// Replace: systemState.fanRPM

// Find: fanSpeed
// Replace: systemState.fanSpeed
```

#### PSU Monitoring

```cpp
// Find: psuVoltage
// Replace: systemState.psuVoltage

// Find: psuMin
// Replace: systemState.psuMin

// Find: psuMax
// Replace: systemState.psuMax
```

#### ADC Sampling

```cpp
// Find: adcSamples
// Replace: systemState.adcSamples

// Find: adcSampleIndex
// Replace: systemState.adcSampleIndex

// Find: adcCurrentSensor
// Replace: systemState.adcCurrentSensor

// Find: lastAdcSample
// Replace: systemState.lastAdcSample

// Find: adcReady
// Replace: systemState.adcReady
```

#### Display & UI

```cpp
// Find: currentMode
// Replace: systemState.currentMode

// Find: lastDisplayUpdate
// Replace: systemState.lastDisplayUpdate

// Find: lastHistoryUpdate
// Replace: systemState.lastHistoryUpdate

// Find: buttonPressStart
// Replace: systemState.buttonPressStart

// Find: buttonPressed
// Replace: systemState.buttonPressed
```

#### Hardware Flags

```cpp
// Find: sdCardAvailable
// Replace: systemState.sdCardAvailable

// Find: rtcAvailable
// Replace: systemState.rtcAvailable

// Find: inAPMode
// Replace: systemState.inAPMode
```

#### Timing

```cpp
// Find: sessionStartTime
// Replace: systemState.sessionStartTime

// Find: lastTachRead
// Replace: systemState.lastTachRead
```

#### FluidNC State

```cpp
// Find: machineState
// Replace: fluidncState.machineState

// Find: fluidncConnected
// Replace: fluidncState.fluidncConnected

// Find: posX
// Replace: fluidncState.posX
// (Repeat for posY, posZ, posA)

// Find: wposX
// Replace: fluidncState.wposX
// (Repeat for wposY, wposZ, wposA)

// Find: wcoX
// Replace: fluidncState.wcoX
// (Repeat for wcoY, wcoZ, wcoA)

// Find: feedRate
// Replace: fluidncState.feedRate

// Find: spindleRPM
// Replace: fluidncState.spindleRPM

// Find: feedOverride
// Replace: fluidncState.feedOverride

// Find: rapidOverride
// Replace: fluidncState.rapidOverride

// Find: spindleOverride
// Replace: fluidncState.spindleOverride

// Find: jobStartTime
// Replace: fluidncState.jobStartTime

// Find: isJobRunning
// Replace: fluidncState.isJobRunning

// Find: autoReportingEnabled
// Replace: fluidncState.autoReportingEnabled

// Find: reportingSetupTime
// Replace: fluidncState.reportingSetupTime

// Find: lastStatusRequest
// Replace: fluidncState.lastStatusRequest

// Find: debugWebSocket
// Replace: fluidncState.debugWebSocket
```

---

### Phase 10: Update ISR Function 🔧

**Location**: tachISR() function

**Change from:**
```cpp
void IRAM_ATTR tachISR() {
  tachCounter++;
}
```

**Change to:**
```cpp
void IRAM_ATTR tachISR() {
  systemState.tachCounter++;
}
```

---

## 🧪 Testing Steps

After making all changes:

### 1. Compile Test
```bash
pio run
```

**Expected**: No compilation errors

### 2. Upload Test
```bash
pio run --target upload
```

**Expected**: Successful upload

### 3. Serial Monitor Test
```bash
pio device monitor
```

**Expected**: Device boots, no crashes

### 4. Web Interface Test
- Open browser to device IP
- Verify main page loads
- Test settings page
- Test admin page
- Test WiFi config page

### 5. API Test
- Test `/api/status` endpoint
- Test `/api/config` endpoint
- Verify data is correct

### 6. Functionality Test
- Verify temperature readings
- Verify fan control
- Verify FluidNC connection
- Verify position updates

---

## 🐛 Troubleshooting

### Compilation Errors

**Error**: `'temperatures' was not declared in this scope`
- **Solution**: You missed updating a variable reference. Search for the variable name and replace with `systemState.temperatures`

**Error**: `undefined reference to 'handleRoot'`
- **Solution**: Make sure you removed the old handler functions and that web_handlers.cpp is being compiled

**Error**: `multiple definition of 'systemState'`
- **Solution**: Check that you only have one instance of the includes, not duplicates

### Runtime Issues

**Issue**: Device crashes on boot
- **Solution**: Check serial output. Likely a null pointer or uninitialized variable

**Issue**: Web pages don't load
- **Solution**: Verify setupWebServer() is being called from web_server.cpp

**Issue**: Data shows as 0 or incorrect
- **Solution**: Verify state initialization is happening in setup()

---

## 📊 Expected Results

### Before Integration
- main.cpp: 1,800+ lines
- 60+ global variables
- All code in one file

### After Integration
- main.cpp: ~500-600 lines (67% reduction)
- 2 state objects (97% reduction in globals)
- Code split across 10+ files
- Much more maintainable!

---

## ✅ Verification Checklist

After integration, verify:

- [ ] Project compiles without errors
- [ ] Device boots successfully
- [ ] Serial output shows no errors
- [ ] Web interface loads
- [ ] All pages accessible (/, /settings, /admin, /wifi)
- [ ] API endpoints return data
- [ ] Temperature readings display
- [ ] Fan control works
- [ ] FluidNC connection works
- [ ] Position data updates
- [ ] Settings can be saved
- [ ] Device can restart via web
- [ ] No memory leaks (monitor heap)

---

## 🎯 Success Criteria

Integration is successful when:

1. ✅ All tests pass
2. ✅ No compilation errors or warnings
3. ✅ Device operates normally
4. ✅ All features work as before
5. ✅ Code is more organized and maintainable

---

## 📞 Support

If you encounter issues:

1. Check the error message carefully
2. Verify you completed all steps
3. Check that includes are correct
4. Verify state objects are initialized
5. Review the troubleshooting section

**Remember**: You have a backup of main.cpp if needed!

---

## 🎉 Completion

Once all tests pass, you've successfully refactored your codebase!

**Congratulations!** Your code is now:
- ✅ More maintainable
- ✅ Better organized
- ✅ Easier to test
- ✅ More professional
- ✅ Ready for future enhancements

---

**Last Updated**: 2025-01-XX  
**Version**: 1.0  
**Status**: Ready for Integration
