# FluidDash Refactoring Instructions for Claude Code

## Project Overview
FluidDash is an ESP32-based CNC machine monitor using a CYD (Cheap Yellow Display) 3.5" touchscreen. The current main.cpp is over 3000 lines and needs to be split into logical, maintainable modules.

## Current Project Structure
- Platform: PlatformIO with ESP32
- Framework: Arduino
- Main file: `src/main.cpp` (3245 lines)
- Key libraries: LovyanGFX, RTClib, WiFiManager, ESPAsyncWebServer, ArduinoJson
- Features: FluidNC monitoring, temperature sensing, fan control, PSU monitoring, JSON screen layouts

## Refactoring Goals

### 1. Preserve ALL Functionality
- Do NOT change any logic or behavior
- Maintain exact same pin configurations
- Keep all global variables accessible where needed
- Preserve all existing features

### 2. Create Clean Module Boundaries
Break the monolithic file into these modules:

```
src/
├── main.cpp                    # ~200 lines: setup() and loop() only
├── config/
│   ├── config.h               # Config struct, enums, constants
│   ├── config.cpp             # loadConfig(), saveConfig()
│   └── pins.h                 # All #define pin assignments
├── display/
│   ├── display.h              # LGFX class and display API
│   ├── display.cpp            # Display initialization & control
│   ├── screen_renderer.h      # JSON screen rendering system
│   ├── screen_renderer.cpp    # drawElement(), drawScreenFromLayout()
│   └── ui_modes.cpp           # Mode handling, button input, mode drawing
├── sensors/
│   ├── sensors.h              # Sensor data structures and API
│   ├── temperature.cpp        # DS18B20 temp reading, ADC sampling
│   ├── fan_control.cpp        # PWM control, tachometer, RPM calc
│   └── psu_monitor.cpp        # PSU voltage ADC monitoring
├── network/
│   ├── wifi_manager.cpp       # WiFi connection, AP mode
│   ├── web_server.cpp         # All web server routes and handlers
│   └── fluidnc_client.cpp     # WebSocket client, status parsing
├── storage/
│   ├── sd_card.cpp            # SD card init and file access
│   └── json_parser.cpp        # Load/parse JSON screen layouts
└── utils/
    ├── rtc.cpp                # RTC initialization and time functions
    └── watchdog.cpp           # Watchdog setup and feeding
```

## Step-by-Step Refactoring Process

### Phase 1: Setup Headers and Pin Definitions
1. Create `src/config/pins.h`
   - Move all #define statements for pins
   - Move all constants (PWM_FREQ, ADC_RESOLUTION, etc.)
   - Move color definitions
   
2. Create `src/config/config.h`
   - Move Config struct
   - Move DisplayMode enum
   - Move all JSON layout structs (ElementType, TextAlign, ScreenElement, ScreenLayout)
   - Add extern declarations for cfg and other config globals

3. Create `src/config/config.cpp`
   - Move initDefaultConfig()
   - Move loadConfig() and saveConfig() functions
   - Include Preferences.h here

**Test after Phase 1:** Compile and verify no errors

### Phase 2: Display Module
4. Create `src/display/display.h`
   - Move LGFX class definition
   - Add function declarations for display control
   - Add extern for gfx object

5. Create `src/display/display.cpp`
   - Move LGFX class implementation
   - Move showSplashScreen()
   - Move any display initialization code
   - Create global: `LGFX gfx;`

6. Create `src/display/screen_renderer.h`
   - Declare parseColor(), parseElementType(), parseAlignment()
   - Declare loadScreenConfig(), drawScreenFromLayout(), drawElement()
   - Declare getDataValue(), getDataString()
   - Declare initDefaultLayouts()

7. Create `src/display/screen_renderer.cpp`
   - Move all JSON parsing functions
   - Move all element drawing functions
   - Move getDataValue() and getDataString() helper functions

8. Create `src/display/ui_modes.cpp`
   - Move drawMonitorMode(), updateMonitorMode()
   - Move drawAlignmentMode(), updateAlignmentMode()
   - Move drawGraphMode(), updateGraphMode()
   - Move drawNetworkMode(), updateNetworkMode()
   - Move drawTempGraph()
   - Move handleButton(), cycleDisplayMode(), showHoldProgress()
   - Move drawScreen() and updateDisplay()

**Test after Phase 2:** Compile and verify

### Phase 3: Sensor Module
9. Create `src/sensors/sensors.h`
   - Declare all sensor-related globals (extern)
   - Declare function prototypes

10. Create `src/sensors/temperature.cpp`
    - Move sampleSensorsNonBlocking()
    - Move processAdcReadings()
    - Move calculateThermistorTemp()
    - Move readTemperatures()
    - Move updateTempHistory()
    - Move allocateHistoryBuffer()
    - Define sensor globals here

11. Create `src/sensors/fan_control.cpp`
    - Move controlFan()
    - Move calculateRPM()
    - Move tachISR()
    - Define fan-related globals

12. Create `src/sensors/psu_monitor.cpp`
    - Move PSU voltage reading logic
    - Define PSU globals

**Test after Phase 3:** Compile and verify

### Phase 4: Network Module
13. Create `src/network/wifi_manager.cpp`
    - Move setupWiFiManager()
    - Move WiFi connection logic
    - Define WiFiManager and related objects

14. Create `src/network/fluidnc_client.cpp`
    - Move connectFluidNC()
    - Move discoverFluidNC()
    - Move fluidNCWebSocketEvent()
    - Move parseFluidNCStatus()
    - Define webSocket and FluidNC state variables

15. Create `src/network/web_server.cpp`
    - Move setupWebServer()
    - Move all HTML generation functions (getMainHTML, getSettingsHTML, etc.)
    - Move all route handlers
    - Define server object

**Test after Phase 4:** Compile and verify

### Phase 5: Storage and Utilities
16. Create `src/storage/sd_card.cpp`
    - Move SD card initialization code
    - Define SD card availability flag

17. Create `src/storage/json_parser.cpp`
    - Already handled in screen_renderer (may merge or keep separate)

18. Create `src/utils/rtc.cpp`
    - Move RTC initialization
    - Move getMonthName()
    - Define rtc object

19. Create `src/utils/watchdog.cpp`
    - Move enableLoopWDT()
    - Move feedLoopWDT()

**Test after Phase 5:** Compile and verify

### Phase 6: Clean Up main.cpp
20. Reduce main.cpp to ONLY:
    - #include statements for all modules
    - setup() function that calls module init functions
    - loop() function that calls module update functions
    - Should be ~150-250 lines maximum

**Final Test:** Full compile and upload to hardware

## Critical Implementation Rules

### 1. Header Guards
Every .h file MUST have header guards:
```cpp
#ifndef MODULENAME_H
#define MODULENAME_H

// content

#endif
```

### 2. Extern vs Definition
- **Headers (.h)**: Use `extern` for global variables
  ```cpp
  extern LGFX gfx;
  extern Config cfg;
  ```
- **Source (.cpp)**: Define the actual variable
  ```cpp
  LGFX gfx;
  Config cfg;
  ```

### 3. Include Order
In each .cpp file:
```cpp
#include <Arduino.h>          // Arduino core first
#include "module.h"           // Own header
#include "config/config.h"    // Config next
#include <ThirdParty.h>       // Third-party libraries
#include "other_modules.h"    // Other project modules
```

### 4. Forward Declarations
Use forward declarations in headers when possible to reduce dependencies:
```cpp
class WebSocketsClient;  // Forward declaration
extern WebSocketsClient webSocket;  // Extern declaration
```

### 5. Global Variables Strategy
- Keep truly global state in appropriate modules
- Use extern in headers for cross-module access
- Consider adding getter/setter functions for better encapsulation (future refactor)

### 6. Testing Between Steps
- **MUST compile** after each major phase
- Don't move to next phase if current phase has errors
- If errors occur, fix them before proceeding
- Use verbose compilation to see all warnings

## PlatformIO Build Commands
```bash
# Clean build
pio run -t clean

# Compile only
pio run

# Upload and monitor
pio run -t upload && pio device monitor
```

## Common Pitfalls to Avoid

1. **Missing includes**: Each .cpp file must include ALL headers it uses
2. **Circular dependencies**: Use forward declarations to break cycles
3. **Multiple definitions**: Never define global variables in headers (use extern)
4. **ISR functions**: Must be defined with IRAM_ATTR in .cpp file, declared in .h
5. **Static class members**: Remember to define them outside the class
6. **Namespace pollution**: Consider adding a namespace for project-specific code (optional)

## Module Dependencies

```
main.cpp
  ├─ config/*          (no dependencies)
  ├─ display/*         (depends on config)
  ├─ sensors/*         (depends on config)
  ├─ network/*         (depends on config, sensors, display for data)
  ├─ storage/*         (depends on config, display for layouts)
  └─ utils/*           (minimal dependencies)
```

## Verification Checklist

After refactoring, verify:
- [ ] Project compiles with no errors
- [ ] Project compiles with no warnings (or expected warnings only)
- [ ] All sensors still read correctly
- [ ] Display shows all screens properly
- [ ] WiFi connects successfully
- [ ] Web interface loads and works
- [ ] FluidNC connection works
- [ ] SD card JSON layouts load
- [ ] RTC time displays correctly
- [ ] Fan control operates
- [ ] PSU monitoring works
- [ ] Button press cycles modes
- [ ] All features function as before

## Notes for Claude Code

- Work methodically through each phase
- Test compilation after EACH phase before moving on
- If you encounter an error, analyze it and fix before proceeding
- Ask the user for clarification if the code structure is ambiguous
- Preserve all comments, especially hardware-specific notes
- Keep the same code style and naming conventions
- Document any changes or decisions you make

## Final Deliverable

The refactored project should:
1. Compile successfully
2. Function identically to the original
3. Have clear module boundaries
4. Be easier to maintain and extend
5. Have each module under 500 lines (ideally under 300)
6. Include this refactoring documentation in a REFACTOR_NOTES.md file

Good luck with the refactoring! This is a substantial but very worthwhile improvement to the codebase.
