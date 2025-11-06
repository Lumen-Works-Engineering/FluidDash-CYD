# FluidDash-CYD Refactoring Progress

## Overview
This document tracks the refactoring of main.cpp into modular components as recommended in the codebase review.

**Date Started**: 2025-01-XX  
**Status**: IN PROGRESS (Phase 1 Complete)

---

## Objectives

1. **Refactor main.cpp** - Split 1800+ line file into modular components
2. **Extract HTML templates** - Move to separate files for easier maintenance
3. **Create state management classes** - Reduce global variables

---

## Progress Summary

### ✅ Completed Tasks

#### 1. Directory Structure Created
- ✅ `src/web/` - Web server modules
- ✅ `src/state/` - State management classes
- ✅ `data/web/` - HTML template storage

#### 2. State Management Classes (COMPLETE)

**SystemState Class** (`src/state/system_state.h/cpp`)
- Encapsulates all system-level runtime state
- **Includes:**
  - Temperature monitoring (sensors, history, peaks)
  - PSU voltage monitoring (current, min, max)
  - Fan control (RPM, speed, tachometer)
  - ADC sampling (non-blocking)
  - Display & UI state
  - Hardware availability flags
  - Timing variables

**Methods:**
- `init()` - Initialize with defaults
- `resetPeakTemps()` - Reset peak temperature tracking
- `resetPSUMinMax()` - Reset PSU min/max values
- `getMaxTemp()` - Get maximum current temperature
- `updatePeakTemps()` - Update peak values
- `updatePSUMinMax()` - Update PSU min/max
- `allocateTempHistory(size)` - Allocate history buffer
- `freeTempHistory()` - Free history buffer
- `addTempToHistory(temp)` - Add reading to history

**FluidNCState Class** (`src/state/fluidnc_state.h/cpp`)
- Encapsulates all FluidNC CNC controller state
- **Includes:**
  - Machine state (IDLE, RUN, ALARM, etc.)
  - Position data (MPos, WPos, WCO)
  - Motion parameters (feed rate, spindle RPM)
  - Override values (feed, rapid, spindle)
  - Job status and timing
  - WebSocket reporting state

**Methods:**
- `init()` - Initialize with defaults
- `setMachineState(state)` - Update machine state
- `isRunning()` - Check if machine is running
- `isAlarmed()` - Check if in alarm state
- `isIdle()` - Check if idle
- `setConnected(bool)` - Update connection status
- `updateMachinePosition(x,y,z,a)` - Update MPos
- `updateWorkPosition(x,y,z,a)` - Update WPos
- `updateWorkOffsets(x,y,z,a)` - Update WCO
- `updateMotion(feed, spindle)` - Update motion params
- `updateOverrides(feed, rapid, spindle)` - Update overrides
- `startJob()` - Start job tracking
- `stopJob()` - Stop job tracking
- `getJobRuntime()` - Get job runtime in seconds

#### 3. Web API Module (COMPLETE)

**WebAPI Module** (`src/web/web_api.h/cpp`)
- JSON response generators for REST API
- **Functions:**
  - `getConfigJSON()` - Configuration data
  - `getStatusJSON()` - System status data
  - `getRTCJSON()` - RTC time data
  - `getUploadStatusJSON()` - Storage status

**Benefits:**
- Uses ArduinoJson for efficient serialization
- Reduces heap fragmentation vs string concatenation
- Cleaner separation of concerns

---

## 🚧 Remaining Tasks

### Phase 2: Web Server Modules

#### WebHandlers Module (TODO)
**File**: `src/web/web_handlers.h/cpp`

**Purpose**: Handle all HTTP request handlers

**Functions to Extract from main.cpp:**
```cpp
void handleRoot();
void handleSettings();
void handleAdmin();
void handleWiFi();
void handleAPIConfig();
void handleAPIStatus();
void handleAPISave();
void handleAPIAdminSave();
void handleAPIResetWiFi();
void handleAPIRestart();
void handleAPIWiFiConnect();
void handleAPIReloadScreens();
void handleAPIRTC();
void handleAPIRTCSet();
void handleUpload();
void handleUploadJSON();
void handleUploadComplete();
void handleUploadStatus();
void handleGetJSON();
void handleSaveJSON();
void handleEditor();
```

**Estimated Size**: ~400 lines

#### WebServer Module (TODO)
**File**: `src/web/web_server.h/cpp`

**Purpose**: Web server setup and routing

**Functions to Extract:**
```cpp
void setupWebServer();  // Register all routes
void initWebServer();   // Initialize server
```

**Estimated Size**: ~100 lines

### Phase 3: HTML Template Extraction

#### HTML Templates (TODO)
**Location**: `data/web/`

**Files to Create:**
- `main.html` - Main dashboard page
- `settings.html` - Settings configuration page
- `admin.html` - Admin/calibration page
- `wifi.html` - WiFi configuration page
- `upload.html` - JSON upload page
- `editor.html` - JSON editor page (currently disabled)

**Current State**: HTML is embedded in PROGMEM strings in main.cpp  
**Target State**: Separate HTML files served from SPIFFS/SD

#### HTMLPages Module (TODO)
**File**: `src/web/html_pages.h/cpp`

**Purpose**: Load and serve HTML templates from storage

**Functions:**
```cpp
String getMainHTML();
String getSettingsHTML();
String getAdminHTML();
String getWiFiConfigHTML();
String loadHTMLTemplate(const char* filename);
String replaceTemplatePlaceholders(String html, ...);
```

**Benefits:**
- Easier HTML editing (syntax highlighting, validation)
- No recompilation needed for HTML changes
- Cleaner C++ code

### Phase 4: Main.cpp Integration

#### Update main.cpp (TODO)
**Changes Required:**

1. **Add Includes:**
```cpp
#include "state/system_state.h"
#include "state/fluidnc_state.h"
#include "web/web_server.h"
#include "web/web_handlers.h"
#include "web/web_api.h"
#include "web/html_pages.h"
```

2. **Replace Global Variables:**
```cpp
// OLD:
float temperatures[4] = {0};
String machineState = "OFFLINE";
// ... 40+ more globals

// NEW:
// Use systemState.temperatures[4]
// Use fluidncState.machineState
```

3. **Update Function Calls:**
```cpp
// OLD:
temperatures[0] = readSensor(0);
machineState = "IDLE";

// NEW:
systemState.temperatures[0] = readSensor(0);
fluidncState.setMachineState("IDLE");
```

4. **Simplify setup():**
```cpp
void setup() {
    // ... existing hardware init ...
    
    // Initialize state objects
    systemState.init();
    fluidncState.init();
    
    // Setup web server (now in separate module)
    setupWebServer();
    
    // ... rest of setup ...
}
```

5. **Remove Extracted Code:**
- Delete all web handler functions (moved to web_handlers.cpp)
- Delete setupWebServer() function (moved to web_server.cpp)
- Delete HTML PROGMEM strings (moved to data/web/)
- Delete getConfigJSON(), getStatusJSON() (moved to web_api.cpp)

**Expected Result**: main.cpp reduced from 1800+ lines to ~600-800 lines

---

## Migration Guide

### For Existing Code Using Global Variables

**Temperature Access:**
```cpp
// OLD:
float temp = temperatures[0];
temperatures[0] = 25.5f;

// NEW:
float temp = systemState.temperatures[0];
systemState.temperatures[0] = 25.5f;
```

**FluidNC State Access:**
```cpp
// OLD:
if (machineState == "RUN") { ... }
posX = 10.5f;

// NEW:
if (fluidncState.isRunning()) { ... }
// or
if (fluidncState.machineState == "RUN") { ... }
fluidncState.posX = 10.5f;
```

**PSU Voltage:**
```cpp
// OLD:
psuVoltage = 12.1f;
if (psuVoltage < psuMin) psuMin = psuVoltage;

// NEW:
systemState.psuVoltage = 12.1f;
systemState.updatePSUMinMax();
```

**Temperature History:**
```cpp
// OLD:
tempHistory[historyIndex] = temp;
historyIndex = (historyIndex + 1) % historySize;

// NEW:
systemState.addTempToHistory(temp);
```

---

## Benefits of Refactoring

### 1. **Improved Maintainability**
- ✅ Smaller, focused files (easier to understand)
- ✅ Clear separation of concerns
- ✅ Easier to locate and fix bugs

### 2. **Better Testability**
- ✅ State classes can be unit tested independently
- ✅ Web handlers can be tested with mock state
- ✅ Reduced coupling between components

### 3. **Enhanced Readability**
- ✅ No more 1800-line files
- ✅ Related functionality grouped together
- ✅ Clear module boundaries

### 4. **Easier Collaboration**
- ✅ Multiple developers can work on different modules
- ✅ Reduced merge conflicts
- ✅ Clearer code ownership

### 5. **Future Extensibility**
- ✅ Easy to add new state variables
- ✅ Easy to add new API endpoints
- ✅ Easy to add new web pages

---

## File Size Comparison

### Before Refactoring
```
main.cpp: 1800+ lines
```

### After Refactoring (Projected)
```
main.cpp: ~600-800 lines (setup, loop, core logic)

src/state/
  system_state.h: ~120 lines
  system_state.cpp: ~120 lines
  fluidnc_state.h: ~160 lines
  fluidnc_state.cpp: ~110 lines

src/web/
  web_api.h: ~40 lines
  web_api.cpp: ~100 lines
  web_handlers.h: ~60 lines
  web_handlers.cpp: ~400 lines
  web_server.h: ~30 lines
  web_server.cpp: ~100 lines
  html_pages.h: ~40 lines
  html_pages.cpp: ~150 lines

data/web/
  main.html: ~100 lines
  settings.html: ~150 lines
  admin.html: ~200 lines
  wifi.html: ~100 lines
  upload.html: ~80 lines
```

**Total Lines**: ~2,760 lines (vs 1800 in single file)  
**But**: Much better organized, maintainable, and testable!

---

## Next Steps

### Immediate (Phase 2)
1. ✅ Create WebHandlers module
2. ✅ Create WebServer module
3. ✅ Test compilation with new modules

### Short-term (Phase 3)
4. ✅ Extract HTML templates to data/web/
5. ✅ Create HTMLPages module
6. ✅ Update web handlers to use HTMLPages

### Final (Phase 4)
7. ✅ Update main.cpp to use all new modules
8. ✅ Remove old code from main.cpp
9. ✅ Test all functionality
10. ✅ Update documentation

---

## Testing Checklist

After completing refactoring, verify:

- [ ] Device boots successfully
- [ ] Web interface loads correctly
- [ ] All web pages display properly
- [ ] API endpoints return correct data
- [ ] Settings can be saved
- [ ] Admin calibration works
- [ ] WiFi configuration works
- [ ] JSON upload works
- [ ] Temperature monitoring works
- [ ] Fan control works
- [ ] FluidNC connection works
- [ ] Position display updates
- [ ] No memory leaks
- [ ] No compilation warnings

---

## Notes

- All new modules use proper header guards
- All classes have constructors and initialization methods
- Global instances created for backward compatibility
- ArduinoJson used for efficient JSON serialization
- Memory management improved (proper cleanup in destructors)
- Code follows existing project style

---

## Questions or Issues?

If you encounter any issues during integration:

1. Check that all includes are correct
2. Verify global instances are declared (systemState, fluidncState)
3. Update any direct global variable access to use state objects
4. Check for typos in variable names (e.g., `temperatures` vs `systemState.temperatures`)

---

**Last Updated**: 2025-01-XX  
**Phase**: 1 of 4 Complete  
**Next Milestone**: Complete Phase 2 (Web Server Modules)
