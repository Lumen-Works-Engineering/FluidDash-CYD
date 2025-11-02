# Claude Code Task: Phase 2 - Screen Modularization & JSON Config

## Prerequisites
✅ Phase 1 complete (CYD hardware conversion done)
✅ Code compiles and runs on CYD
✅ Display shows all 4 modes correctly
✅ SD card slot is functional (test with simple SD test sketch if needed)

## Objective
Refactor FluidDash screen drawing code to be modular and configurable via JSON files stored on SD card. This allows changing screen layouts without recompiling.

## Overview

**Current State:**
- Screen drawing code uses hardcoded coordinates, sizes, colors
- Layout changes require code modification and recompilation
- Draw functions are ~200-300 lines each with embedded layout values

**Target State:**
- Screen layouts defined in JSON files on SD card
- Drawing code references JSON config for all positioning/styling
- Consolidated, reusable drawing primitives
- Easy to create new screen layouts by editing JSON

## Architecture

```
SD Card: /screens/
├── monitor.json      # Monitor mode layout
├── alignment.json    # Alignment mode layout  
├── graph.json        # Graph mode layout
└── network.json      # Network mode layout

Code Structure:
├── Screen element structures (ScreenElement, ScreenLayout)
├── JSON parsing functions (loadScreenConfig)
├── Generic drawing primitives (drawElement, drawLabel, drawValue)
├── Mode-specific update functions (use loaded config)
```

## Task List

### Task 1: Add Required Libraries
**File:** `platformio.ini`
**Location:** `lib_deps` section

**Add:**
```ini
lib_deps =
    adafruit/RTClib@^2.1.4
    lovyan03/LovyanGFX@^1.1.16
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/AsyncTCP.git
    https://github.com/tzapu/WiFiManager.git
    links2004/WebSockets@2.7.1
    bblanchon/ArduinoJson@^7.0.0        # ADD THIS
```

### Task 2: Add SD Card and JSON Includes
**File:** `src/main.cpp`
**Location:** After existing includes (around line 46)

**Add:**
```cpp
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>
```

### Task 3: Define Screen Element Structures
**File:** `src/main.cpp`
**Location:** After the Config struct definition (around line 168)

**Add these structures:**
```cpp
// ========== SCREEN LAYOUT STRUCTURES ==========

// Element types
enum ElementType {
  ELEM_RECT,
  ELEM_TEXT_STATIC,
  ELEM_TEXT_DYNAMIC,
  ELEM_LINE,
  ELEM_TEMP_VALUE,
  ELEM_COORD_VALUE,
  ELEM_STATUS_VALUE
};

// Single screen element
struct ScreenElement {
  ElementType type;
  int16_t x, y, w, h;
  uint16_t color;
  uint16_t bgColor;
  uint8_t textSize;
  char label[32];           // For static text or label
  char dataSource[32];      // Variable name to display (e.g. "tempX", "posX")
  uint8_t decimals;         // For numeric values
  bool filled;              // For rectangles
};

// Complete screen layout
struct ScreenLayout {
  char name[32];
  ScreenElement elements[50];  // Max 50 elements per screen
  uint8_t elementCount;
};

// Global layout storage (one for each display mode)
ScreenLayout monitorLayout;
ScreenLayout alignmentLayout;
ScreenLayout graphLayout;
ScreenLayout networkLayout;
```

### Task 4: Add SD Card Pin Definition
**File:** `src/main.cpp`
**Location:** After the BTN_MODE definition (around line 77)

**Add:**
```cpp
// SD Card (Pre-wired onboard - VSPI bus)
#define SD_CS       5     // SD chip select
#define SD_MOSI     23    // SPI MOSI (VSPI)
#define SD_SCK      18    // SPI clock (VSPI)
#define SD_MISO     19    // SPI MISO (VSPI)
```

### Task 5: Add Global SD Card Variable
**File:** `src/main.cpp`
**Location:** After WiFiManager declaration (around line 227)

**Add:**
```cpp
bool sdCardAvailable = false;
```

### Task 6: Create JSON Parsing Function
**File:** `src/main.cpp`
**Location:** Before setup() function (around line 325, after function prototypes)

**Add:**
```cpp
// ========== SCREEN CONFIG FUNCTIONS ==========

bool loadScreenConfig(const char* filename, ScreenLayout& layout) {
  if (!sdCardAvailable) {
    Serial.println("SD card not available, using defaults");
    return false;
  }

  File file = SD.open(filename);
  if (!file) {
    Serial.printf("Failed to open %s\n", filename);
    return false;
  }

  // Allocate JSON document
  JsonDocument doc;
  
  // Parse JSON
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("Failed to parse %s: %s\n", filename, error.c_str());
    return false;
  }

  // Extract layout name
  strlcpy(layout.name, doc["name"] | "Unnamed", sizeof(layout.name));

  // Parse elements array
  JsonArray elements = doc["elements"];
  layout.elementCount = 0;

  for (JsonObject elem : elements) {
    if (layout.elementCount >= 50) break;  // Safety limit

    ScreenElement& se = layout.elements[layout.elementCount];

    // Parse element type
    const char* typeStr = elem["type"];
    if (strcmp(typeStr, "rect") == 0) se.type = ELEM_RECT;
    else if (strcmp(typeStr, "text") == 0) se.type = ELEM_TEXT_STATIC;
    else if (strcmp(typeStr, "dynamic") == 0) se.type = ELEM_TEXT_DYNAMIC;
    else if (strcmp(typeStr, "line") == 0) se.type = ELEM_LINE;
    else if (strcmp(typeStr, "temp") == 0) se.type = ELEM_TEMP_VALUE;
    else if (strcmp(typeStr, "coord") == 0) se.type = ELEM_COORD_VALUE;
    else if (strcmp(typeStr, "status") == 0) se.type = ELEM_STATUS_VALUE;
    else continue;  // Unknown type, skip

    // Parse position and size
    se.x = elem["x"] | 0;
    se.y = elem["y"] | 0;
    se.w = elem["w"] | 0;
    se.h = elem["h"] | 0;

    // Parse colors (hex string to uint16_t)
    const char* colorStr = elem["color"];
    se.color = colorStr ? strtol(colorStr, NULL, 16) : COLOR_TEXT;
    
    const char* bgColorStr = elem["bgColor"];
    se.bgColor = bgColorStr ? strtol(bgColorStr, NULL, 16) : COLOR_BG;

    // Parse text properties
    se.textSize = elem["size"] | 1;
    strlcpy(se.label, elem["label"] | "", sizeof(se.label));
    strlcpy(se.dataSource, elem["data"] | "", sizeof(se.dataSource));
    se.decimals = elem["decimals"] | 0;
    se.filled = elem["filled"] | true;

    layout.elementCount++;
  }

  Serial.printf("Loaded %s: %d elements\n", layout.name, layout.elementCount);
  return true;
}

void initDefaultLayouts() {
  // Create minimal default layouts if SD card fails
  // Monitor mode default
  strcpy(monitorLayout.name, "Monitor (Default)");
  monitorLayout.elementCount = 0;  // Will use legacy draw functions
  
  // Similar for other modes
  strcpy(alignmentLayout.name, "Alignment (Default)");
  alignmentLayout.elementCount = 0;
  
  strcpy(graphLayout.name, "Graph (Default)");
  graphLayout.elementCount = 0;
  
  strcpy(networkLayout.name, "Network (Default)");
  networkLayout.elementCount = 0;
}
```

### Task 7: Create Generic Drawing Function
**File:** `src/main.cpp`
**Location:** After loadScreenConfig function

**Add:**
```cpp
// Generic element drawing based on config
void drawElement(const ScreenElement& elem) {
  char buffer[64];

  switch (elem.type) {
    case ELEM_RECT:
      if (elem.filled) {
        gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.color);
      } else {
        gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);
      }
      break;

    case ELEM_TEXT_STATIC:
      gfx.setTextSize(elem.textSize);
      gfx.setTextColor(elem.color);
      gfx.setCursor(elem.x, elem.y);
      gfx.print(elem.label);
      break;

    case ELEM_LINE:
      if (elem.w > elem.h) {
        gfx.drawFastHLine(elem.x, elem.y, elem.w, elem.color);
      } else {
        gfx.drawFastVLine(elem.x, elem.y, elem.h, elem.color);
      }
      break;

    case ELEM_TEMP_VALUE:
      // Parse data source like "temp0", "temp1", etc.
      if (strncmp(elem.dataSource, "temp", 4) == 0) {
        int index = atoi(elem.dataSource + 4);
        if (index >= 0 && index < 4) {
          gfx.setTextSize(elem.textSize);
          
          // Color based on threshold
          uint16_t color = (temperatures[index] > cfg.temp_threshold_high) ? COLOR_WARN : elem.color;
          gfx.setTextColor(color);
          
          gfx.setCursor(elem.x, elem.y);
          sprintf(buffer, "%s%d%s", elem.label, (int)temperatures[index], 
                  cfg.use_fahrenheit ? "F" : "C");
          gfx.print(buffer);
        }
      }
      break;

    case ELEM_COORD_VALUE:
      // Parse data source like "wposX", "mposY", etc.
      {
        float value = 0;
        bool isMpos = (strncmp(elem.dataSource, "mpos", 4) == 0);
        char axis = elem.dataSource[isMpos ? 4 : 4];  // Get axis letter
        
        if (isMpos) {
          if (axis == 'X') value = posX;
          else if (axis == 'Y') value = posY;
          else if (axis == 'Z') value = posZ;
        } else {
          if (axis == 'X') value = wposX;
          else if (axis == 'Y') value = wposY;
          else if (axis == 'Z') value = wposZ;
        }
        
        gfx.setTextSize(elem.textSize);
        gfx.setTextColor(elem.color);
        gfx.setCursor(elem.x, elem.y);
        
        if (elem.decimals == 3) {
          sprintf(buffer, "%s%7.3f", elem.label, value);
        } else {
          sprintf(buffer, "%s%6.2f", elem.label, value);
        }
        gfx.print(buffer);
      }
      break;

    case ELEM_STATUS_VALUE:
      // Handle various status displays
      gfx.setTextSize(elem.textSize);
      gfx.setTextColor(elem.color);
      gfx.setCursor(elem.x, elem.y);
      
      if (strcmp(elem.dataSource, "fan") == 0) {
        sprintf(buffer, "%s%d%% (%dRPM)", elem.label, fanSpeed, fanRPM);
        gfx.print(buffer);
      } else if (strcmp(elem.dataSource, "psu") == 0) {
        sprintf(buffer, "%s%.1fV", elem.label, psuVoltage);
        gfx.print(buffer);
      } else if (strcmp(elem.dataSource, "fluidnc") == 0) {
        if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
        else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
        sprintf(buffer, "%s%s", elem.label, machineState.c_str());
        gfx.print(buffer);
      }
      break;

    default:
      break;
  }
}

// Draw entire screen from layout
void drawScreenFromLayout(const ScreenLayout& layout) {
  gfx.fillScreen(COLOR_BG);
  
  for (uint8_t i = 0; i < layout.elementCount; i++) {
    drawElement(layout.elements[i]);
  }
}
```

### Task 8: Initialize SD Card in setup()
**File:** `src/main.cpp`
**Location:** In setup() function, after display initialization (around line 365)

**Add:**
```cpp
// Initialize SD card (VSPI bus - separate from display)
feedLoopWDT();
Serial.println("Initializing SD card...");
SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
if (SD.begin(SD_CS)) {
  sdCardAvailable = true;
  Serial.println("SD card initialized");
  
  // Load screen layouts from JSON
  loadScreenConfig("/screens/monitor.json", monitorLayout);
  loadScreenConfig("/screens/alignment.json", alignmentLayout);
  loadScreenConfig("/screens/graph.json", graphLayout);
  loadScreenConfig("/screens/network.json", networkLayout);
} else {
  Serial.println("SD card init failed, using default layouts");
  initDefaultLayouts();
}
```

### Task 9: Update drawScreen() Function
**File:** `src/main.cpp`
**Location:** drawScreen() function (around line 1561)

**Modify to check for JSON layouts first:**
```cpp
void drawScreen() {
  feedLoopWDT();
  
  switch (currentMode) {
    case MODE_MONITOR:
      if (monitorLayout.elementCount > 0) {
        drawScreenFromLayout(monitorLayout);
      } else {
        drawMonitorMode();  // Fallback to legacy
      }
      break;
      
    case MODE_ALIGNMENT:
      if (alignmentLayout.elementCount > 0) {
        drawScreenFromLayout(alignmentLayout);
      } else {
        drawAlignmentMode();
      }
      break;
      
    case MODE_GRAPH:
      if (graphLayout.elementCount > 0) {
        drawScreenFromLayout(graphLayout);
      } else {
        drawGraphMode();
      }
      break;
      
    case MODE_NETWORK:
      if (networkLayout.elementCount > 0) {
        drawScreenFromLayout(networkLayout);
      } else {
        drawNetworkMode();
      }
      break;
  }
}
```

### Task 10: Create Example JSON Config Files

Create these files on the SD card in `/screens/` directory:

**File:** `monitor.json`
```json
{
  "name": "Monitor",
  "elements": [
    {
      "type": "rect",
      "x": 0,
      "y": 0,
      "w": 480,
      "h": 25,
      "color": "001F",
      "filled": true
    },
    {
      "type": "text",
      "x": 10,
      "y": 6,
      "size": 2,
      "color": "FFFF",
      "label": "FluidDash"
    },
    {
      "type": "line",
      "x": 0,
      "y": 25,
      "w": 480,
      "h": 1,
      "color": "4208"
    },
    {
      "type": "text",
      "x": 10,
      "y": 30,
      "size": 1,
      "color": "FFFF",
      "label": "DRIVERS:"
    },
    {
      "type": "temp",
      "x": 50,
      "y": 47,
      "size": 2,
      "color": "07FF",
      "label": "X:",
      "data": "temp0"
    },
    {
      "type": "temp",
      "x": 50,
      "y": 77,
      "size": 2,
      "color": "07FF",
      "label": "YL:",
      "data": "temp1"
    },
    {
      "type": "temp",
      "x": 50,
      "y": 107,
      "size": 2,
      "color": "07FF",
      "label": "YR:",
      "data": "temp2"
    },
    {
      "type": "temp",
      "x": 50,
      "y": 137,
      "size": 2,
      "color": "07FF",
      "label": "Z:",
      "data": "temp3"
    },
    {
      "type": "line",
      "x": 0,
      "y": 175,
      "w": 480,
      "h": 1,
      "color": "4208"
    },
    {
      "type": "status",
      "x": 10,
      "y": 200,
      "size": 1,
      "color": "4208",
      "label": "Fan: ",
      "data": "fan"
    },
    {
      "type": "status",
      "x": 10,
      "y": 215,
      "size": 1,
      "color": "4208",
      "label": "PSU: ",
      "data": "psu"
    },
    {
      "type": "status",
      "x": 10,
      "y": 230,
      "size": 1,
      "color": "07FF",
      "label": "FluidNC: ",
      "data": "fluidnc"
    }
  ]
}
```

**Note:** Create similar JSON files for `alignment.json`, `graph.json`, and `network.json` based on desired layouts.

### Task 11: Add Config Reload Function (Optional)
**File:** `src/main.cpp`
**Location:** After drawScreenFromLayout function

**Add:**
```cpp
// Reload all screen configs (useful for live updates)
void reloadScreenConfigs() {
  if (!sdCardAvailable) return;
  
  Serial.println("Reloading screen configs...");
  loadScreenConfig("/screens/monitor.json", monitorLayout);
  loadScreenConfig("/screens/alignment.json", alignmentLayout);
  loadScreenConfig("/screens/graph.json", graphLayout);
  loadScreenConfig("/screens/network.json", networkLayout);
  
  // Redraw current screen
  drawScreen();
}
```

**Add web endpoint for reload:**
```cpp
// In setupWebServer() function, add:
server.on("/api/reload-screens", HTTP_GET, [](AsyncWebServerRequest *request) {
  reloadScreenConfigs();
  request->send(200, "text/plain", "Screen configs reloaded");
});
```

## Verification Steps

### 1. Compilation
```bash
pio run
```
Should compile without errors. Watch for:
- ArduinoJson library installed
- SD.h included properly
- No syntax errors in JSON parsing

### 2. SD Card Setup
1. Format SD card as FAT32
2. Create `/screens/` directory
3. Copy `monitor.json` (and other JSON files) to `/screens/`
4. Insert SD card into CYD

### 3. Serial Monitor Testing
```bash
pio device monitor
```
Look for:
```
Initializing SD card...
SD card initialized
Loaded Monitor: 13 elements
```

If SD card fails:
```
SD card init failed, using default layouts
```
(Should still work with legacy draw functions)

### 4. Display Testing
- Monitor mode should display using JSON layout
- If JSON missing, falls back to original drawMonitorMode()
- Layouts should match JSON coordinates exactly

### 5. Live Config Testing
1. Edit `monitor.json` on PC
2. Update SD card
3. Reload card or call `/api/reload-screens`
4. Screen should update without recompiling

## Migration Strategy

**Gradual Migration Recommended:**

1. **Week 1:** Get Phase 2 working with monitor.json only
2. **Week 2:** Create and test alignment.json
3. **Week 3:** Create graph.json and network.json
4. **Week 4:** Refine and optimize

**Fallback Safety:**
- Legacy draw functions remain in code
- If JSON fails to load, uses original hard-coded layouts
- Zero risk of breaking existing functionality

## Advanced Features (Optional)

After basic JSON system works, consider:

### 1. Dynamic Element Updates
Instead of full screen redraws, update only changed elements:
```cpp
void updateElement(const ScreenElement& elem) {
  // Clear area
  gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.bgColor);
  // Redraw element
  drawElement(elem);
}
```

### 2. Multiple Layout Files
Support theme switching:
```
/screens/
├── themes/
│   ├── default/
│   │   ├── monitor.json
│   │   └── ...
│   └── high-contrast/
│       ├── monitor.json
│       └── ...
```

### 3. Layout Editor Web Interface
Create web page to edit layouts visually and save to SD card

## Expected Outcomes

✅ **Immediate Benefits:**
- Change screen layouts by editing JSON
- No recompilation needed for cosmetic changes
- Easy to create custom layouts
- Consistent drawing behavior

✅ **Long-term Benefits:**
- Multiple users can share layout files
- A/B test different screen designs
- Community-contributed layouts
- Remote layout updates via web interface

## Estimated Time

- **Tasks 1-9:** 30-45 minutes (core functionality)
- **Task 10:** 15-30 minutes per JSON file
- **Task 11:** 10-15 minutes (optional reload)
- **Testing:** 30-60 minutes
- **Total:** 2-3 hours for complete implementation

## Success Criteria

✅ Code compiles with ArduinoJson
✅ SD card initializes successfully
✅ At least one JSON layout loads
✅ Screen displays using JSON layout
✅ Fallback to legacy code works if JSON missing
✅ Can update layout by editing JSON + reloading

---

**Status:** Ready for Claude Code execution (after Phase 1 complete)
**Complexity:** Medium-High
**Risk Level:** Low (has fallback to legacy code)
