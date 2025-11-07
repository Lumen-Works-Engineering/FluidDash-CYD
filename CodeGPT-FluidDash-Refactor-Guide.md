# FluidDash Simplified Refactor - CodeGPT Implementation Guide

## Project Overview

**Goal:** Transform FluidDash into a reliable sensor monitoring dashboard with optional FluidNC integration.

**Core Features:**
- Temperature monitoring (internal controller, ambient, external sensors)
- PSU voltage monitoring
- PWM fan control based on temperature
- Optional FluidNC data display (when WiFi configured)

**Architecture Changes:**
- Remove SD card complexity → SPIFFS-only
- Remove JSON layout system → Clean C++ screen files
- Fix WebSocket blocking → Stable operation
- Make FluidNC optional → Sensors work standalone

---

## Phase 1: Critical Fixes (Stop Boot Loops)

### Fix 1: WebSocket Blocking in loop()

**File:** `src/main.cpp`

**Find and replace:**

**OLD CODE (around line 392-407):**
```cpp
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
```

**NEW CODE:**
```cpp
  if (WiFi.status() == WL_CONNECTED) {
      yield();  // Yield before WebSocket operations
      
      // Only call webSocket.loop() frequently when connected
      // When disconnected, limit reconnection attempts to prevent blocking
      if (fluidncConnected) {
          // Connected: safe to call every loop
          webSocket.loop();
          yield();
      } else {
          // Disconnected: only attempt reconnection every 5 seconds
          if (millis() - lastConnectionAttempt >= 5000) {
              Serial.println("[FluidNC] Attempting reconnection...");
              webSocket.loop();  // Try to connect
              lastConnectionAttempt = millis();
              yield();
          }
      }

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
```

**Add new variable (around line 110):**

**After this line:**
```cpp
unsigned long lastStatusRequest = 0;
```

**Add:**
```cpp
unsigned long lastConnectionAttempt = 0;  // Track WebSocket connection attempts
```

---

## Phase 2: Remove Storage Complexity

### Change 1: Remove StorageManager

**Files to DELETE:**
- `src/storage_manager.h`
- `src/storage_manager.cpp`

### Change 2: Simplify setup() - Remove SD Card

**File:** `src/main.cpp`

**Find and DELETE entire section (around line 239-280):**
```cpp
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
```

**REPLACE WITH:**
```cpp
    // ========== INITIALIZE SPIFFS ==========
    feedLoopWDT();
    Serial.println("\n=== Initializing SPIFFS ===");
    
    if (!SPIFFS.begin(true)) {  // true = format if mount fails
        Serial.println("ERROR: SPIFFS mount failed!");
        Serial.println("Continuing with default config...");
    } else {
        Serial.println("SUCCESS: SPIFFS mounted");
        size_t totalBytes = SPIFFS.totalBytes();
        size_t usedBytes = SPIFFS.usedBytes();
        Serial.printf("  Total: %d bytes\n", totalBytes);
        Serial.printf("  Used: %d bytes (%.1f%%)\n", usedBytes, (usedBytes * 100.0) / totalBytes);
    }
    Serial.println("=== SPIFFS Ready ===\n");
    // ========== END SPIFFS INIT ==========
```

### Change 3: Remove JSON Layout Loading

**File:** `src/main.cpp`

**Find and DELETE entire section (around line 281-321):**
```cpp
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
        // Add debug output
        Serial.println("\n=== JSON Layout Status ===");
        Serial.printf("  monitor.isValid:    %s\n", monitorLayout.isValid ? "TRUE" : "FALSE");
        Serial.printf("  alignment.isValid:  %s\n", alignmentLayout.isValid ? "TRUE" : "FALSE");
        Serial.printf("  graph.isValid:      %s\n", graphLayout.isValid ? "TRUE" : "FALSE");
        Serial.printf("  network.isValid:    %s\n", networkLayout.isValid ? "TRUE" : "FALSE");
        Serial.println("========================\n");
        Serial.println("=== JSON Layout Loading Complete ===\n");
    } else {
        Serial.println("[JSON] No storage available (SD/SPIFFS), using legacy drawing\n");
    }
    // ========== END PHASE 2 LAYOUT LOADING ==========
```

**DELETE THIS** (we'll use direct screen functions instead)

### Change 4: Remove JSON-related includes

**File:** `src/main.cpp`

**Find and DELETE:**
```cpp
#include "storage_manager.h"
#include "screen_renderer.h"
```

### Change 5: Remove JSON-related globals

**File:** `src/main.cpp`

**Find and DELETE (around line 50-70):**
```cpp
// Storage
StorageManager storage;
bool sdCardAvailable = false;

// Screen layouts
ScreenLayout monitorLayout;
ScreenLayout alignmentLayout;
ScreenLayout graphLayout;
ScreenLayout networkLayout;
bool layoutsLoaded = false;
```

---

## Phase 3: Create New Screen System

### Create New Screen Files

**Create:** `src/screens/screen_common.h`

```cpp
#ifndef SCREEN_COMMON_H
#define SCREEN_COMMON_H

#include <Arduino_GFX_Library.h>

// ========================================
// COMMON SCREEN UTILITIES
// Shared functions for all screens
// ========================================

// Color scheme
#define COLOR_BG           TFT_BLACK
#define COLOR_HEADER       TFT_WHITE
#define COLOR_TEMP_NORMAL  TFT_CYAN
#define COLOR_TEMP_WARN    TFT_YELLOW
#define COLOR_TEMP_ALERT   TFT_RED
#define COLOR_VOLTAGE_OK   TFT_GREEN
#define COLOR_VOLTAGE_LOW  TFT_RED
#define COLOR_FAN          TFT_BLUE
#define COLOR_BORDER       0x4A49  // Dark gray

// Temperature thresholds
#define TEMP_WARN_C   45.0
#define TEMP_ALERT_C  55.0

// Voltage thresholds
#define VOLTAGE_MIN  11.0
#define VOLTAGE_MAX  13.0

// Helper function prototypes
uint16_t getTempColor(float temp);
uint16_t getVoltageColor(float voltage);
void drawBox(int x, int y, int w, int h, uint16_t color);
void drawLabel(int x, int y, const char* label, uint16_t color);

// Helper implementations
inline uint16_t getTempColor(float temp) {
    if (temp >= TEMP_ALERT_C) return COLOR_TEMP_ALERT;
    if (temp >= TEMP_WARN_C) return COLOR_TEMP_WARN;
    return COLOR_TEMP_NORMAL;
}

inline uint16_t getVoltageColor(float voltage) {
    if (voltage < VOLTAGE_MIN || voltage > VOLTAGE_MAX) return COLOR_VOLTAGE_LOW;
    return COLOR_VOLTAGE_OK;
}

inline void drawBox(int x, int y, int w, int h, uint16_t color) {
    extern Arduino_GFX* gfx;
    gfx->drawRect(x, y, w, h, color);
}

inline void drawLabel(int x, int y, const char* label, uint16_t color) {
    extern Arduino_GFX* gfx;
    gfx->setTextColor(color, COLOR_BG);
    gfx->setTextSize(1);
    gfx->setCursor(x, y);
    gfx->print(label);
}

#endif
```

---

**Create:** `src/screens/screen_monitor.h`

```cpp
#ifndef SCREEN_MONITOR_H
#define SCREEN_MONITOR_H

#include "screen_common.h"

// ========================================
// MAIN MONITORING SCREEN
// Shows all temperatures, voltage, and fan
// ========================================

void drawMonitorScreen() {
    extern Arduino_GFX* gfx;
    
    // Clear screen
    gfx->fillScreen(COLOR_BG);
    
    // ===== HEADER =====
    gfx->setTextColor(COLOR_HEADER, COLOR_BG);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print("SENSOR MONITOR");
    
    // ===== INTERNAL TEMPERATURES =====
    drawTempBox(10, 50, 220, 80, "Controller", temp_controller);
    drawTempBox(250, 50, 220, 80, "Ambient", temp_ambient);
    
    // ===== EXTERNAL TEMPERATURES =====
    drawTempBox(10, 150, 220, 80, "External 1", temp_ext1);
    drawTempBox(250, 150, 220, 80, "External 2", temp_ext2);
    
    // ===== PSU VOLTAGE =====
    drawVoltageBox(10, 250, 220, 50, "PSU", voltage_psu);
    
    // ===== COOLING FAN =====
    drawFanBox(250, 250, 220, 50, "Fan", fan_pwm_percent);
}

// Temperature display box
void drawTempBox(int x, int y, int w, int h, const char* label, float temp) {
    extern Arduino_GFX* gfx;
    
    uint16_t color = getTempColor(temp);
    
    // Border
    drawBox(x, y, w, h, color);
    
    // Label
    drawLabel(x + 5, y + 5, label, color);
    
    // Temperature value
    gfx->setTextColor(color, COLOR_BG);
    gfx->setTextSize(3);
    gfx->setCursor(x + 10, y + 30);
    gfx->printf("%.1f", temp);
    
    // Units
    gfx->setTextSize(2);
    gfx->print(" C");
}

// Voltage display box
void drawVoltageBox(int x, int y, int w, int h, const char* label, float volts) {
    extern Arduino_GFX* gfx;
    
    uint16_t color = getVoltageColor(volts);
    
    // Border
    drawBox(x, y, w, h, color);
    
    // Label
    drawLabel(x + 5, y + 5, label, color);
    
    // Voltage value
    gfx->setTextColor(color, COLOR_BG);
    gfx->setTextSize(2);
    gfx->setCursor(x + 10, y + 25);
    gfx->printf("%.2fV", volts);
}

// Fan display box
void drawFanBox(int x, int y, int w, int h, const char* label, int percent) {
    extern Arduino_GFX* gfx;
    
    // Border
    drawBox(x, y, w, h, COLOR_FAN);
    
    // Label
    drawLabel(x + 5, y + 5, label, COLOR_FAN);
    
    // Fan speed
    gfx->setTextColor(COLOR_FAN, COLOR_BG);
    gfx->setTextSize(2);
    gfx->setCursor(x + 10, y + 25);
    gfx->printf("%d%%", percent);
}

#endif
```

---

**Create:** `src/screens/screen_fluidnc.h`

```cpp
#ifndef SCREEN_FLUIDNC_H
#define SCREEN_FLUIDNC_H

#include "screen_common.h"

// ========================================
// FLUIDNC DATA SCREEN (Optional)
// Only shown when FluidNC is connected
// ========================================

void drawFluidNCScreen() {
    extern Arduino_GFX* gfx;
    extern bool fluidncConnected;
    
    // Clear screen
    gfx->fillScreen(COLOR_BG);
    
    // ===== HEADER =====
    gfx->setTextColor(COLOR_HEADER, COLOR_BG);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print("FLUIDNC STATUS");
    
    // Show connection status
    if (!fluidncConnected) {
        gfx->setTextColor(COLOR_TEMP_ALERT, COLOR_BG);
        gfx->setTextSize(2);
        gfx->setCursor(10, 50);
        gfx->print("Not Connected");
        
        gfx->setTextSize(1);
        gfx->setCursor(10, 80);
        gfx->print("Configure WiFi to enable");
        return;
    }
    
    // ===== MACHINE STATE =====
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_HEADER, COLOR_BG);
    gfx->setCursor(10, 50);
    gfx->print("State:");
    
    gfx->setTextSize(2);
    gfx->setCursor(80, 45);
    extern String machineState;
    gfx->print(machineState);
    
    // ===== POSITION DATA =====
    gfx->setTextSize(1);
    gfx->setCursor(10, 100);
    gfx->print("Position (MPos):");
    
    gfx->setTextSize(1);
    extern float posX, posY, posZ, posA;
    
    gfx->setCursor(10, 120);
    gfx->printf("X: %.3f", posX);
    
    gfx->setCursor(10, 140);
    gfx->printf("Y: %.3f", posY);
    
    gfx->setCursor(10, 160);
    gfx->printf("Z: %.3f", posZ);
    
    gfx->setCursor(10, 180);
    gfx->printf("A: %.3f", posA);
}

#endif
```

---

### Update ui_modes Files

**File:** `src/ui_modes.h`

**REPLACE entire file with:**

```cpp
#ifndef UI_MODES_H
#define UI_MODES_H

// UI mode definitions
enum UIMode {
    MODE_MONITOR,     // Main sensor monitoring screen
    MODE_FLUIDNC,     // FluidNC status (if connected)
    MODE_CONFIG       // Configuration screen
};

extern UIMode currentMode;

// Screen management functions
void drawScreen();
void updateScreen();
void switchMode(UIMode newMode);

#endif
```

---

**File:** `src/ui_modes.cpp`

**REPLACE entire file with:**

```cpp
#include "ui_modes.h"
#include "display.h"
#include "screens/screen_monitor.h"
#include "screens/screen_fluidnc.h"

// Current UI mode
UIMode currentMode = MODE_MONITOR;

// Draw current screen (full redraw)
void drawScreen() {
    switch(currentMode) {
        case MODE_MONITOR:
            drawMonitorScreen();
            break;
            
        case MODE_FLUIDNC:
            drawFluidNCScreen();
            break;
            
        case MODE_CONFIG:
            // TODO: Configuration screen
            break;
    }
}

// Update dynamic elements only
void updateScreen() {
    // For now, just redraw
    // Later: optimize to update only changed values
    drawScreen();
    yield();
}

// Switch to different mode
void switchMode(UIMode newMode) {
    if (currentMode != newMode) {
        currentMode = newMode;
        drawScreen();
    }
}
```

---

## Phase 4: Update main.cpp

### Add sensor variables

**File:** `src/main.cpp`

**Add after line 80 (after FluidNC variables):**

```cpp
// ========== SENSOR DATA ==========
// Temperature sensors
float temp_controller = 0.0;  // Internal controller temp
float temp_ambient = 0.0;     // Ambient case temp
float temp_ext1 = 0.0;        // External sensor 1
float temp_ext2 = 0.0;        // External sensor 2

// Voltage monitoring
float voltage_psu = 12.0;     // PSU voltage

// Fan control
int fan_pwm_percent = 0;      // Fan PWM duty cycle (0-100%)
```

### Add screen includes

**File:** `src/main.cpp`

**Replace screen includes with:**

```cpp
#include "ui_modes.h"
#include "screens/screen_common.h"
```

### Simplify loop()

**File:** `src/main.cpp`

**Replace updateScreen() section with:**

```cpp
  // Update screen every second
  if (millis() - lastDisplayUpdate >= 1000) {
      updateScreen();
      lastDisplayUpdate = millis();
      yield();
  }
```

---

## Phase 5: Remove JSON-related Files

**DELETE these files:**
- `src/screen_renderer.h`
- `src/screen_renderer.cpp`
- `data/screens/monitor.json`
- `data/screens/alignment.json`
- `data/screens/graph.json`
- `data/screens/network.json`

---

## Testing & Verification

### Step 1: Build and Upload

```bash
pio run --target upload
```

### Step 2: Monitor Serial Output

Expected output:
```
=== FluidDash Startup ===
Display initialized
=== Initializing SPIFFS ===
SUCCESS: SPIFFS mounted
  Total: 1310720 bytes
  Used: 0 bytes (0.0%)
=== SPIFFS Ready ===

WiFi Connected!
IP: 192.168.1.100
mDNS started: http://fluiddash.local
[FluidNC] Attempting reconnection...
Setup complete - entering main loop
```

### Step 3: Verify Behavior

**With FluidNC Offline:**
- ✅ Clean boot, no loops
- ✅ Screen displays sensor data
- ✅ Every 5 seconds: "[FluidNC] Attempting reconnection..."
- ✅ No watchdog timeouts

**With FluidNC Online:**
- ✅ Clean boot
- ✅ WebSocket connects
- ✅ Sensor data displays
- ✅ Can switch to FluidNC screen

---

## Phase 6: Adding Real Sensor Support

### Next Steps (After Refactor Works)

1. **Add temperature sensor reading** (DS18B20, DHT22, etc.)
2. **Add voltage monitoring** (ADC with voltage divider)
3. **Add PWM fan control** (based on temperature thresholds)
4. **Add screen switching** (button press or touch)

---

## Benefits of This Refactor

### Before:
- ❌ 8+ boot loops before stabilizing
- ❌ Complex JSON parsing
- ❌ Dual storage (SD + SPIFFS)
- ❌ WebSocket blocking
- ❌ Hard to edit screens
- ❌ Focus on FluidNC integration

### After:
- ✅ Single clean boot
- ✅ No JSON overhead
- ✅ Simple SPIFFS-only
- ✅ Non-blocking WebSocket
- ✅ **EASY to edit screens** (just C++ files!)
- ✅ Focus on sensor monitoring
- ✅ FluidNC as optional bonus

### Screen Editing Workflow:

**To modify monitor screen:**
1. Open `src/screens/screen_monitor.h`
2. Change coordinates, colors, or layout
3. Save file
4. Build and upload
5. **Instant feedback - no JSON errors!**

**Example - Change temperature box position:**
```cpp
// Old:
drawTempBox(10, 50, 220, 80, "Controller", temp_controller);

// New (move down):
drawTempBox(10, 100, 220, 80, "Controller", temp_controller);
```

**That's it!** Compiler checks everything, no runtime surprises!

---

## Summary

This refactor:
1. **Fixes boot loops** (WebSocket throttling)
2. **Simplifies storage** (SPIFFS-only)
3. **Removes JSON** (clean C++ screens)
4. **Enables easy editing** (just change numbers in .h files)
5. **Focuses on sensors** (your actual goal!)
6. **Makes FluidNC optional** (bonus feature)

**Result:** Stable, reliable system that boots fast and is EASY to customize! 🎉
