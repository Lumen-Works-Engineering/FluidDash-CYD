# Tomorrow's Session Notes - FluidDash CYD

**Date Created**: November 3, 2025
**Project**: FluidDash-CYD ESP32 Temperature Monitor

---

## Current Status Summary

### ✅ Completed Today
1. **DS18B20 OneWire Sensor Implementation**
   - Added OneWire and DallasTemperature libraries to platformio.ini
   - Implemented sensor initialization in sensors.cpp
   - Added SensorMapping struct to sensors.h for future sensor configuration
   - Sensors successfully detecting on GPIO 21 with 4.7kΩ pull-up resistor
   - Temperature readings working (confirmed by user)
   - Memory usage: RAM 75,352 bytes (23.0%), Flash 1,241,357 bytes (94.7%)

### ❌ Issues Remaining

#### 1. **CRITICAL: Upload Page Crash** (High Priority)
**Problem**: `/upload-json` endpoint crashes when user clicks upload button

**What We've Tried**:
- Disabled `/get-json` endpoint (was using static File variable with chunked response)
- Enhanced error handling in `/upload-json` handler
- Added file close checks and write error detection
- Disabled `/save-json` and `/editor` endpoints

**Current State**: Still crashing - needs deeper investigation

**Location**: [src/main.cpp:1082-1128](src/main.cpp#L1082-L1128)

**Next Steps to Try**:
1. Check if SD card operations are thread-safe with AsyncWebServer
2. Add mutex/semaphore for SD card access during uploads
3. Consider using a different approach - store to SPIFFS first, then copy to SD
4. Add more detailed serial debug logging to pinpoint exact crash location
5. Check if watchdog timer is interfering
6. Review ESPAsyncWebServer upload examples for best practices

**Previous Crash Log** (from earlier session):
```
Guru Meditation Error: Core 1 panic'ed (LoadProhibited). Exception was unhandled.
EXCVADDR: 0x0000000E
```

#### 2. **Font Display Issue** (Medium Priority)
**Problem**: Fonts appear blocky/pixelated since switching to JSON screen layouts

**Analysis**:
- LovyanGFX uses bitmap fonts by default
- Scaling bitmap fonts with `setTextSize(2)` or `setTextSize(3)` causes pixelation
- Old hardcoded display used same font sizes (1 and 2)
- JSON layouts use size 2 and 3, making pixelation more noticeable
- User confirmed font size 1 is "too small no matter what"

**Location**: [src/display/screen_renderer.cpp:246-355](src/display/screen_renderer.cpp#L246-L355)

**Solutions to Consider**:
1. **Option A (Quick)**: Adjust JSON file font sizes
   - Change size 3 → size 2 for less critical text
   - May not fully resolve issue

2. **Option B (Better)**: Implement LovyanGFX smooth fonts
   - Use built-in fonts like `fonts::Font2`, `fonts::FreeSans9pt7b`, etc.
   - Requires code changes in screen_renderer.cpp
   - Need to add font selection logic based on size parameter
   - Will provide smooth, anti-aliased rendering at all sizes

---

## Hardware Configuration

### DS18B20 Sensors
- **Pin**: GPIO 21 (ONE_WIRE_BUS_1)
- **Pull-up**: 4.7kΩ resistor to 3.3V
- **Resolution**: 12-bit (0.0625°C precision)
- **Mode**: Non-blocking (setWaitForConversion(false))
- **Status**: ✅ Working - sensors detected and reading temperatures

### CYD Hardware
- **Display**: ILI9341 480x320 (3.5" or 4.0")
- **Touch**: XPT2046
- **SD Card**: VSPI bus (CS=GPIO5, MOSI=23, SCK=18, MISO=19)
- **COM Port**: COM9

---

## Phase 1 Implementation Status

From [SENSOR_MANAGEMENT_IMPLEMENTATION.md](SENSOR_MANAGEMENT_IMPLEMENTATION.md):

### Phase 1: Basic DS18B20 Reading
- ✅ Add libraries to platformio.ini
- ✅ Define OneWire pin in pins.h (was already defined)
- ✅ Add SensorMapping struct to sensors.h
- ✅ Implement DS18B20 initialization
- ✅ Implement basic temperature reading
- ✅ Replace placeholder temps with real readings
- ⏳ Testing (partially complete - sensors work, but upload crash prevents full testing)

### Pending Phases (Not Started)
- Phase 2: Sensor Discovery & Identification
- Phase 3: Configuration Management (SD card)
- Phase 4: Web API Integration
- Phase 5: Configuration UI
- Phase 6: Display Integration
- Phase 7: Testing & Documentation

---

## Code Changes Made Today

### Files Modified
1. **platformio.ini** - Added DS18B20 libraries:
   ```ini
   paulstoffregen/OneWire@^2.3.8
   milesburton/DallasTemperature@^3.11.0
   ```

2. **src/sensors/sensors.h** - Added SensorMapping struct:
   ```cpp
   struct SensorMapping {
       uint8_t uid[8];           // 64-bit DS18B20 ROM address
       char friendlyName[32];    // "X-Axis Motor"
       char alias[16];           // "temp0"
       bool enabled;
       char notes[64];           // Optional user notes
   };
   ```

3. **src/sensors/sensors.cpp** - Implemented DS18B20 reading:
   - `initDS18B20Sensors()` - Initialize and detect sensors
   - `processAdcReadings()` - Read temps from sensors (replaced dummy 25°C values)
   - `getTempByAlias()` - Get temp by sensor alias
   - `getTempByUID()` - Get temp by sensor UID
   - `getSensorCount()` - Return number of configured sensors

4. **src/main.cpp** - Added sensor initialization call in setup()

### Files Modified (Attempts to Fix Upload Crash)
- **src/main.cpp** lines 1082-1211:
  - Enhanced `/upload-json` error handling
  - Disabled `/get-json` endpoint
  - Disabled `/save-json` endpoint
  - Disabled `/editor` page

---

## Documentation Created

1. **DS18B20_PHASE1_IMPLEMENTATION.md** (800+ lines)
   - Complete Phase 1 implementation details
   - Before/after code comparisons
   - Hardware specs and wiring
   - Memory usage analysis
   - Testing instructions
   - Troubleshooting guide

2. **DYNAMIC_DATA_VARIABLES_AND_API_REFERENCE.md** (1,200+ lines)
   - 66+ runtime variables documented
   - 16 API endpoints detailed
   - 25 data source IDs for JSON layouts
   - Usage examples (JavaScript, cURL, Python)
   - Home Assistant integration YAML
   - Troubleshooting guide

---

## Tomorrow's Action Plan

### Priority 1: Fix Upload Crash ⚠️
**Goal**: Resolve the `/upload-json` crash to enable JSON file uploads

**Debug Steps**:
1. Add detailed serial logging to `/upload-json` handler:
   ```cpp
   Serial.printf("[Upload] Chunk received: index=%zu, len=%zu, final=%d\n", index, len, final);
   Serial.printf("[Upload] Free heap: %d bytes\n", ESP.getFreeHeap());
   ```

2. Check if SD card access needs synchronization:
   - Add semaphore/mutex for SD operations
   - Review if AsyncWebServer callbacks are thread-safe with SD

3. Test alternative approach:
   - Write to SPIFFS/LittleFS first
   - Then copy to SD card after upload completes
   - Reduces SD card contention during async operations

4. Review ESPAsyncWebServer documentation for upload best practices

5. Check if watchdog timer needs feeding during uploads

6. Test with smaller JSON file first to isolate size-related issues

### Priority 2: Implement Smooth Fonts 🎨
**Goal**: Replace bitmap fonts with smooth rendered fonts

**Implementation**:
1. Research LovyanGFX font options:
   - `fonts::Font0` through `fonts::Font8` (built-in)
   - `fonts::FreeSans9pt7b`, `fonts::FreeSans12pt7b`, etc. (FreeFont)
   - Check memory requirements for each

2. Modify [screen_renderer.cpp](src/display/screen_renderer.cpp):
   - Add font selection logic based on textSize
   - Map size 1 → small font, size 2 → medium font, size 3 → large font
   - Update `drawElement()` function to set font before drawing

3. Test font rendering:
   - Check readability at different sizes
   - Verify performance impact
   - Measure memory usage increase

### Priority 3: Complete Phase 1 Testing ✅
**Goal**: Verify all Phase 1 functionality works correctly

**Tests**:
1. Verify temperature readings are accurate
2. Check peak temperature tracking
3. Confirm temperatures display correctly on screen
4. Test with multiple sensors (up to 4)
5. Verify sensor UID detection and printing to serial

---

## Useful File Locations

### Core Files
- Main application: [src/main.cpp](src/main.cpp)
- Sensor logic: [src/sensors/sensors.cpp](src/sensors/sensors.cpp)
- Sensor headers: [src/sensors/sensors.h](src/sensors/sensors.h)
- Display rendering: [src/display/screen_renderer.cpp](src/display/screen_renderer.cpp)
- Pin definitions: [src/config/pins.h](src/config/pins.h)

### Upload Handler
- Location: [src/main.cpp:1082-1128](src/main.cpp#L1082-L1128)
- Current status: Enhanced but still crashing

### Font Rendering
- Location: [src/display/screen_renderer.cpp:225-360](src/display/screen_renderer.cpp#L225-L360)
- Current: Using bitmap fonts with setTextSize()
- Needs: Switch to TrueType-style fonts

### JSON Layouts
- Directory: [screens/](screens/)
- Active file: [screens/monitor.json](screens/monitor.json)

---

## Build Information

### Last Successful Build
- **Date**: November 3, 2025
- **Time**: ~49 seconds
- **RAM**: 75,352 bytes (23.0% of 327,680 bytes)
- **Flash**: 1,241,357 bytes (94.7% of 1,310,720 bytes)
- **Status**: ✅ SUCCESS

### Flash Memory Status
⚠️ **WARNING**: Flash usage at 94.7% - approaching limit
- Consider PROGMEM optimization if adding more features
- Monitor after adding smooth fonts (may use more Flash)

---

## Serial Monitor Output to Check

When debugging tomorrow, look for these messages:

### DS18B20 Sensor Initialization
```
[SENSORS] Initializing DS18B20 sensors...
[SENSORS] Found X DS18B20 sensor(s) on bus
[SENSORS] Sensor 0 UID: XX:XX:XX:XX:XX:XX:XX:XX
[SENSORS] DS18B20 initialization complete
```

### Upload Handler
```
[Upload] Starting: filename.json
[Upload] Complete: filename.json
```
or error messages if failing

---

## Questions to Consider

1. **Upload Crash**:
   - Is AsyncWebServer's file upload safe with SD card operations?
   - Do we need to disable watchdog during uploads?
   - Should we buffer the upload in RAM first?

2. **Font Quality**:
   - Which LovyanGFX fonts provide best quality/size ratio?
   - How much Flash will smooth fonts consume?
   - Should we allow font selection in JSON config?

3. **Sensor Management**:
   - Continue with Phase 2 (discovery) after fixing crashes?
   - Or focus on stability and fonts first?

---

## Git Status

### Modified Files (Uncommitted)
- platformio.ini
- src/sensors/sensors.h
- src/sensors/sensors.cpp
- src/main.cpp (upload handler changes)

### Untracked Files
- CLAUDE_CODE_REFACTOR_INSTRUCTIONS.md
- FluidDash-CYD.code-workspace
- FluidDash_CYD_Wiring_guide.md
- FluidDash_code_structure.md
- QUICK_START_GUIDE.md
- REFACTORING_CHECKLIST.md
- REFACTORING_TEMPLATES.md
- TROUBLESHOOTING_GUIDE.md
- DS18B20_PHASE1_IMPLEMENTATION.md
- DYNAMIC_DATA_VARIABLES_AND_API_REFERENCE.md

### Deleted Files
- 251021-085358-esp32dev.code-workspace

### Recent Commits
```
06e181f DS18B20 sensors implemented
b0019a0 Working on screen design
0932a98 Still working on screen design
666af95 Working on screen layout json
```

### Suggested Commit Message (After Fixes)
```
Fix upload crash and implement smooth fonts

- Resolve AsyncWebServer SD card upload crash
- Implement LovyanGFX smooth font rendering
- Improve error handling in upload endpoint
- Add detailed debug logging for troubleshooting

Fixes #[issue] if applicable
```

---

## References & Documentation

### LovyanGFX
- GitHub: https://github.com/lovyan03/LovyanGFX
- Font examples: Check `examples/HowToUse/3_fonts/` in library
- Built-in fonts: `lgfx/v1/lgfx_fonts.hpp`

### ESPAsyncWebServer
- GitHub: https://github.com/me-no-dev/ESPAsyncWebServer
- Upload example: Check `examples/simple_upload/`

### DS18B20
- Datasheet: https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
- DallasTemperature library: https://github.com/milesburton/Arduino-Temperature-Control-Library

---

## Notes for Claude

When continuing this session tomorrow:

1. **First thing**: Focus on fixing the upload crash - it's blocking testing
2. **Read this file first** to get up to speed quickly
3. **Check serial output** if user can provide it - will help debug upload crash
4. **Font implementation** is secondary but important for usability
5. **Don't start Phase 2** until Phase 1 is fully tested and stable
6. **Memory is tight** - Flash at 94.7%, be careful adding large features

---

## Success Criteria for Tomorrow

### Minimum Goals
- [ ] Upload page works without crashing
- [ ] Can upload JSON files to SD card successfully
- [ ] DS18B20 temperatures verified accurate

### Stretch Goals
- [ ] Smooth fonts implemented and looking good
- [ ] Phase 1 fully tested and documented
- [ ] Ready to start Phase 2 (sensor discovery)

---

**Good luck tomorrow! 🚀**
