# Refactoring Troubleshooting Guide

Common issues you might encounter during the FluidDash refactoring and how to fix them.

## Compilation Errors

### Error: "multiple definition of 'variableName'"

**Cause:** Variable defined in a header file that's included by multiple .cpp files

**Example:**
```cpp
// display.h - WRONG
LGFX gfx;  // ❌ This will cause multiple definition errors
```

**Fix:**
```cpp
// display.h - CORRECT
extern LGFX gfx;  // ✅ Declaration only

// display.cpp - CORRECT  
#include "display.h"
LGFX gfx;  // ✅ Definition in ONE .cpp file
```

**Rule:** Headers (.h) = extern declarations | Implementation (.cpp) = definitions

---

### Error: "undefined reference to 'functionName'"

**Cause:** Function declared but not defined, or missing object file in build

**Check:**
1. Is the function actually implemented in a .cpp file?
2. Is the .cpp file in the src/ directory (or a subdirectory)?
3. Does the function signature match the declaration exactly?

**Example of mismatch:**
```cpp
// sensors.h
void readTemperature(int sensorNum);  // Declaration

// sensors.cpp
void readTemperature(uint8_t sensorNum) { }  // ❌ Type mismatch!
```

**Fix:** Make signatures identical:
```cpp
// sensors.h
void readTemperature(uint8_t sensorNum);  // ✅

// sensors.cpp  
void readTemperature(uint8_t sensorNum) { }  // ✅
```

---

### Error: "'ClassName' does not name a type"

**Cause:** Missing include or forward declaration

**Fix:** Add the necessary include at the top of the file
```cpp
#include <LovyanGFX.hpp>  // Must include before using LGFX
```

**For circular dependencies, use forward declarations:**
```cpp
// In header file
class FluidNCClient;  // Forward declaration
extern FluidNCClient fluidnc;  // Can now use pointer/reference
```

---

### Error: "no matching function for call to 'ClassName::ClassName()'"

**Cause:** Class has no default constructor but trying to create instance without arguments

**Fix:** Provide constructor arguments or add default constructor
```cpp
// If you see this for: LGFX gfx;
// Make sure LGFX class has: LGFX() { } constructor
```

---

### Error: "expected initializer before 'ClassName'"

**Cause:** Missing semicolon or malformed class/struct definition

**Check:** All struct/class definitions end with semicolon
```cpp
struct Config {
  int value;
};  // ⬅ Don't forget this semicolon!
```

---

### Error: "ISR not in IRAM"

**Full error:** `Guru Meditation Error: Core 1 panic'ed (Cache disabled but cached memory region accessed)`

**Cause:** Interrupt Service Routine (ISR) not marked with IRAM_ATTR

**Fix:**
```cpp
// sensors.h
void IRAM_ATTR tachISR();  // ✅ Mark with IRAM_ATTR

// sensors.cpp
void IRAM_ATTR tachISR() {  // ✅ Also in definition
  tachCounter++;
}
```

---

## Linking Errors

### Error: "cannot open source file 'path/to/file.h'"

**Cause:** Incorrect include path

**PlatformIO automatically includes:**
- Everything in `src/` and subdirectories
- Everything in `include/`
- Libraries in `lib/`

**Fix:** Use relative paths from src/
```cpp
// If file structure is: src/display/display.h
// From main.cpp:
#include "display/display.h"  // ✅

// From display/ui_modes.cpp:
#include "display.h"  // ✅ Same directory
#include "../config/config.h"  // ✅ Parent directory
```

---

## Runtime Errors

### Issue: Display shows garbage or doesn't initialize

**Possible causes:**
1. LovyanGFX initialization moved but not called in setup()
2. Display pins misconfigured during refactor
3. Bus configuration wrong

**Fix:**
```cpp
// In main.cpp setup():
gfx.init();  // Must call this!
gfx.setRotation(1);  // And set rotation
```

---

### Issue: Sensors read as 0 or NaN

**Possible causes:**
1. Sensor initialization not called
2. ADC pins not configured  
3. Global variables not initialized

**Check:**
```cpp
// In sensors.cpp, make sure globals are initialized:
float temperatures[4] = {0};  // ✅ Initialize to zero
float peakTemps[4] = {0};     // ✅

// In main.cpp setup():
initSensors();  // Must call initialization!
```

---

### Issue: WebSocket doesn't connect

**Possible causes:**
1. webSocket object defined in wrong module
2. WiFi not connected before WebSocket init
3. Event handler not registered

**Check order in main.cpp setup():**
```cpp
void setup() {
  // ...
  setupWiFiManager();     // 1. WiFi first
  setupWebServer();       // 2. Web server second  
  connectFluidNC();       // 3. WebSocket last
}
```

---

### Issue: SD card "not found"

**Possible causes:**
1. SPI bus conflict with display
2. Wrong CS pin
3. SD init called before SPI init

**Fix:**
```cpp
// SD card uses VSPI, display uses HSPI - should be fine
// Make sure to begin SPI explicitly:
SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
if (!SD.begin(SD_CS)) {
  Serial.println("SD init failed");
}
```

---

## Logic Errors

### Issue: Some features work, others don't

**Diagnosis process:**
1. Check what was moved where - might have missed calling a function
2. Look at main.cpp loop() - are all module update functions called?
3. Check timing - were millis() checks properly moved?

**Example - common oversight:**
```cpp
// In loop(), you need to call ALL update functions:
void loop() {
  sampleSensorsNonBlocking();  // ✅ 
  updateDisplay();              // ✅
  webSocketLoop();              // ❌ Forgot this? WebSocket won't work!
  handleButton();               // ✅
}
```

---

### Issue: Variables are always 0 or default value

**Cause:** Using wrong instance of variable (defined twice)

**Example:**
```cpp
// sensors.h
extern float temperature;  // Declaration

// sensors.cpp  
float temperature = 0;  // Definition ✅

// temperature.cpp - WRONG
float temperature = 0;  // ❌ Another definition! Creates separate variable!
```

**Fix:** Only define variable in ONE .cpp file, all others just include the header

---

## PlatformIO Specific Issues

### Issue: Build takes forever or seems stuck

**Cause:** Including header files from wrong directories

**Check platformio.ini:**
```ini
[env:esp32dev]
build_flags = 
    -I src
    -I include
# Don't add random include paths
```

---

### Issue: Upload fails with "No module named 'serial'"

**Fix:** Install pyserial
```bash
pip install pyserial
# or
pio pkg install --tool "tool-esptoolpy"
```

---

### Issue: "A fatal error occurred: Could not open port"

**Causes:**
1. Serial monitor is open - close it!
2. Wrong USB port selected
3. Permission issue

**Fix:**
```bash
# Check available ports:
pio device list

# Specify port in platformio.ini:
[env:esp32dev]
upload_port = /dev/ttyUSB0  # Linux
upload_port = COM3           # Windows
```

---

## Prevention Strategies

### Use Version Control
```bash
# Before starting:
git init
git add .
git commit -m "Before refactoring"

# After each phase:
git add .
git commit -m "Phase 1 complete: headers and config"
```

### Test Incrementally
Don't move everything at once! After each phase:
```bash
pio run -t clean
pio run
# If it compiles, continue. If not, fix before proceeding.
```

### Keep Notes
Document what you're doing:
```cpp
// sensors.cpp
// Moved from main.cpp lines 450-680
// Functions: readTemperatures, processADC, updateHistory
```

---

## Getting Unstuck

### Strategy 1: Compiler Error Analysis
Read the error carefully:
- **First line** usually has the actual error
- **File and line number** tells you where
- **Error message** tells you what

Example:
```
src/display/display.cpp:45:10: error: 'cfg' was not declared in this scope
   45 |   gfx.setBrightness(cfg.brightness);
```
Translation: "In display.cpp line 45, you used 'cfg' but didn't include config.h"

Fix: Add `#include "../config/config.h"` at top of display.cpp

---

### Strategy 2: Simplify
If totally stuck:
1. Comment out the problematic section
2. Make sure everything else compiles
3. Uncomment line by line to find the issue

---

### Strategy 3: Compare with Original
Look at your main.cpp.backup:
- How was it done before?
- What includes were used?
- What was the order of operations?

---

### Strategy 4: Check Dependencies
Create a mental map:
```
display module needs:
  - config (for cfg.brightness)
  - pins (for TFT_CS, etc.)
  
sensors module needs:
  - config (for cfg.temp_offset_x)
  - Arduino.h (for Serial, pinMode, etc.)
  
config module needs:
  - Arduino.h (for String, etc.)
  - Nothing else!
```

If module A needs something from module B, module A must include module B's header.

---

## Quick Checklist for Each New File

When creating a new .h file:
- [ ] Header guards (#ifndef, #define, #endif)
- [ ] Necessary includes
- [ ] Forward declarations if needed
- [ ] Struct/class definitions
- [ ] extern for global variables
- [ ] Function declarations

When creating a new .cpp file:
- [ ] Include own .h file first
- [ ] Include other necessary headers
- [ ] Define global variables (no extern here)
- [ ] Implement functions
- [ ] Preserve IRAM_ATTR on ISRs

---

## Still Stuck?

1. **Google the exact error message** - someone has seen it before
2. **Check PlatformIO forums** - lots of ESP32 experts
3. **Ask Claude Code** - "I'm getting this error: [paste full error]. What does it mean?"
4. **Ask me** - I'm happy to help debug specific issues

Remember: Refactoring is an iterative process. It's normal to encounter issues. The key is fixing them one at a time, testing frequently, and not making too many changes before testing.

Good luck! 🎯
