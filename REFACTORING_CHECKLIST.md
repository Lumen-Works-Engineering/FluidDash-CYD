# FluidDash Refactoring Progress Checklist

Use this checklist to track your progress through the refactoring.

## Pre-Refactoring Setup
- [ ] Back up current main.cpp (`cp src/main.cpp src/main.cpp.backup`)
- [ ] If using git, commit current state
- [ ] If using git, create new branch (`git checkout -b refactor-modular`)
- [ ] Verify project compiles successfully before starting
- [ ] Open Claude Code in VS Code
- [ ] Have CLAUDE_CODE_REFACTOR_INSTRUCTIONS.md ready
- [ ] Have terminal visible for compilation output

## Phase 1: Headers and Pin Definitions

### Create Directory Structure
- [ ] Create `src/config/` directory
- [ ] Create `src/display/` directory
- [ ] Create `src/sensors/` directory
- [ ] Create `src/network/` directory
- [ ] Create `src/storage/` directory
- [ ] Create `src/utils/` directory

### Create Config Module Files
- [ ] Create `src/config/pins.h`
  - [ ] Move all #define pin assignments
  - [ ] Move all constants (PWM_FREQ, etc.)
  - [ ] Move color definitions
  - [ ] Add header guards
  
- [ ] Create `src/config/config.h`
  - [ ] Move Config struct
  - [ ] Move DisplayMode enum
  - [ ] Move ElementType enum
  - [ ] Move TextAlign enum
  - [ ] Move ScreenElement struct
  - [ ] Move ScreenLayout struct
  - [ ] Add extern Config cfg
  - [ ] Add function declarations
  - [ ] Add header guards
  
- [ ] Create `src/config/config.cpp`
  - [ ] Include config.h
  - [ ] Include Preferences.h
  - [ ] Define Config cfg
  - [ ] Move initDefaultConfig()
  - [ ] Move loadConfig()
  - [ ] Move saveConfig()

### Test Phase 1
- [ ] Update main.cpp includes
- [ ] Run `pio run -t clean`
- [ ] Run `pio run`
- [ ] Compilation successful ✅
- [ ] Commit: "Phase 1 complete: headers and config"

## Phase 2: Display Module

### Create Display Files
- [ ] Create `src/display/display.h`
  - [ ] Move LGFX class declaration
  - [ ] Add extern LGFX gfx
  - [ ] Add display function declarations
  - [ ] Include necessary headers
  - [ ] Add header guards
  
- [ ] Create `src/display/display.cpp`
  - [ ] Include display.h
  - [ ] Move LGFX class implementation
  - [ ] Define LGFX gfx
  - [ ] Move showSplashScreen()
  - [ ] Add initDisplay() function
  
- [ ] Create `src/display/screen_renderer.h`
  - [ ] Declare parseColor()
  - [ ] Declare parseElementType()
  - [ ] Declare parseAlignment()
  - [ ] Declare loadScreenConfig()
  - [ ] Declare drawScreenFromLayout()
  - [ ] Declare drawElement()
  - [ ] Declare getDataValue()
  - [ ] Declare getDataString()
  - [ ] Declare initDefaultLayouts()
  - [ ] Add header guards
  
- [ ] Create `src/display/screen_renderer.cpp`
  - [ ] Include screen_renderer.h
  - [ ] Move all parseXxx() functions
  - [ ] Move loadScreenConfig()
  - [ ] Move drawScreenFromLayout()
  - [ ] Move drawElement()
  - [ ] Move getDataValue()
  - [ ] Move getDataString()
  - [ ] Move initDefaultLayouts()
  
- [ ] Create `src/display/ui_modes.cpp`
  - [ ] Include necessary headers
  - [ ] Move drawMonitorMode()
  - [ ] Move updateMonitorMode()
  - [ ] Move drawAlignmentMode()
  - [ ] Move updateAlignmentMode()
  - [ ] Move drawGraphMode()
  - [ ] Move updateGraphMode()
  - [ ] Move drawNetworkMode()
  - [ ] Move updateNetworkMode()
  - [ ] Move drawTempGraph()
  - [ ] Move drawScreen()
  - [ ] Move updateDisplay()
  - [ ] Move handleButton()
  - [ ] Move cycleDisplayMode()
  - [ ] Move showHoldProgress()

### Test Phase 2
- [ ] Update main.cpp includes
- [ ] Run `pio run -t clean`
- [ ] Run `pio run`
- [ ] Compilation successful ✅
- [ ] Commit: "Phase 2 complete: display module"

## Phase 3: Sensors Module

### Create Sensor Files
- [ ] Create `src/sensors/sensors.h`
  - [ ] Add extern declarations for temperature arrays
  - [ ] Add extern declarations for fan variables
  - [ ] Add extern declarations for PSU variables
  - [ ] Add extern declarations for ADC variables
  - [ ] Declare all sensor functions
  - [ ] Add header guards
  
- [ ] Create `src/sensors/temperature.cpp`
  - [ ] Include sensors.h
  - [ ] Define temperature arrays
  - [ ] Define tempHistory pointer
  - [ ] Define ADC sampling variables
  - [ ] Move sampleSensorsNonBlocking()
  - [ ] Move processAdcReadings()
  - [ ] Move calculateThermistorTemp()
  - [ ] Move readTemperatures()
  - [ ] Move updateTempHistory()
  - [ ] Move allocateHistoryBuffer()
  
- [ ] Create `src/sensors/fan_control.cpp`
  - [ ] Include sensors.h
  - [ ] Define fan variables (fanRPM, fanSpeed, tachCounter)
  - [ ] Move controlFan()
  - [ ] Move calculateRPM()
  - [ ] Move tachISR() (with IRAM_ATTR!)
  
- [ ] Create `src/sensors/psu_monitor.cpp`
  - [ ] Include sensors.h
  - [ ] Define PSU variables (psuVoltage, psuMin, psuMax)
  - [ ] Move PSU voltage reading logic

### Test Phase 3
- [ ] Update main.cpp includes
- [ ] Run `pio run -t clean`
- [ ] Run `pio run`
- [ ] Compilation successful ✅
- [ ] Commit: "Phase 3 complete: sensors module"

## Phase 4: Network Module

### Create Network Files
- [ ] Create `src/network/wifi_manager.cpp`
  - [ ] Include necessary headers
  - [ ] Define wm (WiFiManager instance)
  - [ ] Move setupWiFiManager()
  - [ ] Move WiFi connection logic
  
- [ ] Create `src/network/fluidnc_client.cpp`
  - [ ] Include necessary headers
  - [ ] Define webSocket
  - [ ] Define FluidNC state variables
  - [ ] Move connectFluidNC()
  - [ ] Move discoverFluidNC()
  - [ ] Move fluidNCWebSocketEvent()
  - [ ] Move parseFluidNCStatus()
  
- [ ] Create `src/network/web_server.cpp`
  - [ ] Include necessary headers
  - [ ] Define server (AsyncWebServer instance)
  - [ ] Move setupWebServer()
  - [ ] Move getMainHTML()
  - [ ] Move getSettingsHTML()
  - [ ] Move getAdminHTML()
  - [ ] Move getWiFiConfigHTML()
  - [ ] Move getConfigJSON()
  - [ ] Move getStatusJSON()
  - [ ] Move all route handlers

### Test Phase 4
- [ ] Update main.cpp includes
- [ ] Run `pio run -t clean`
- [ ] Run `pio run`
- [ ] Compilation successful ✅
- [ ] Commit: "Phase 4 complete: network module"

## Phase 5: Storage and Utilities

### Create Storage Files
- [ ] Create `src/storage/sd_card.cpp`
  - [ ] Include necessary headers
  - [ ] Define sdCardAvailable flag
  - [ ] Move SD card initialization code
  - [ ] Add initSDCard() function
  
- [ ] Create `src/storage/json_parser.cpp` (if not already in screen_renderer)
  - [ ] Move any JSON parsing specific to file loading

### Create Utility Files
- [ ] Create `src/utils/rtc.cpp`
  - [ ] Include RTClib.h
  - [ ] Define rtc object
  - [ ] Move RTC initialization
  - [ ] Move getMonthName()
  - [ ] Add initRTC() function
  
- [ ] Create `src/utils/watchdog.cpp`
  - [ ] Move enableLoopWDT()
  - [ ] Move feedLoopWDT()

### Test Phase 5
- [ ] Update main.cpp includes
- [ ] Run `pio run -t clean`
- [ ] Run `pio run`
- [ ] Compilation successful ✅
- [ ] Commit: "Phase 5 complete: storage and utilities"

## Phase 6: Clean Up main.cpp

### Simplify main.cpp
- [ ] Keep only #include statements
- [ ] Simplify setup() to call init functions
- [ ] Simplify loop() to call update functions
- [ ] Remove all moved code
- [ ] Add clear comments
- [ ] Verify file is under 300 lines

### Final main.cpp should have:
- [ ] All necessary includes
- [ ] Clean setup() function (~30 lines)
- [ ] Clean loop() function (~30 lines)
- [ ] No function definitions (except setup/loop)
- [ ] Clear, logical flow

### Test Phase 6
- [ ] Run `pio run -t clean`
- [ ] Run `pio run`
- [ ] Compilation successful ✅
- [ ] No warnings (or only expected warnings)
- [ ] Commit: "Phase 6 complete: main.cpp simplified"

## Hardware Testing

### Upload and Test
- [ ] Run `pio run -t upload`
- [ ] Upload successful
- [ ] Display initializes
- [ ] Splash screen shows
- [ ] Default screen displays

### Test All Features
- [ ] Temperature sensors read correctly
- [ ] All 4 temperature channels show values
- [ ] Fan control works
- [ ] Fan RPM displays
- [ ] PSU voltage displays correctly
- [ ] RTC time shows (if RTC installed)
- [ ] SD card detected (if card inserted)
- [ ] JSON layout loads from SD
- [ ] Button press cycles modes
- [ ] All display modes work:
  - [ ] Monitor mode
  - [ ] Alignment mode (if implemented)
  - [ ] Graph mode (if implemented)
  - [ ] Network mode (if implemented)

### Test Network Features
- [ ] WiFi connects successfully
- [ ] Web interface accessible
- [ ] Can access settings page
- [ ] Can modify configuration
- [ ] Changes save correctly
- [ ] FluidNC connects
- [ ] FluidNC status updates
- [ ] Machine coordinates display
- [ ] WebSocket stays connected

## Post-Refactoring

### Documentation
- [ ] Update README with new structure
- [ ] Document module purposes
- [ ] Create REFACTOR_NOTES.md with summary
- [ ] Document any issues encountered
- [ ] Document any deviations from plan

### Code Quality
- [ ] All files have header guards
- [ ] All files have descriptive comments
- [ ] No commented-out code blocks
- [ ] Consistent naming conventions
- [ ] Consistent formatting

### Version Control
- [ ] All changes committed
- [ ] Meaningful commit messages
- [ ] Tag release: `git tag v0.2-modular`
- [ ] Merge to main (if using branches)
- [ ] Push to remote (if using remote repo)

### Backup
- [ ] Keep main.cpp.backup for reference
- [ ] Export working binary (.bin file)
- [ ] Document the refactoring date

## Optional Future Improvements
- [ ] Add header comments to each file
- [ ] Create unit tests for modules
- [ ] Add error handling wrappers
- [ ] Implement logging system
- [ ] Add module initialization status checks
- [ ] Consider creating namespaces
- [ ] Add Doxygen documentation
- [ ] Profile code for optimization opportunities

## Notes / Issues Encountered
```
[Space for notes during refactoring]







```

## Time Tracking
- Start time: _______________
- Phase 1 completed: _______________
- Phase 2 completed: _______________
- Phase 3 completed: _______________
- Phase 4 completed: _______________
- Phase 5 completed: _______________
- Phase 6 completed: _______________
- Testing completed: _______________
- Total time: _______________

---

## Success Criteria Met?
- [X] All checkboxes above are checked
- [ ] Project compiles with no errors
- [ ] Project uploads successfully
- [ ] All features work on hardware
- [ ] main.cpp is under 300 lines
- [ ] Each module is under 500 lines
- [ ] Code is well-organized and maintainable
- [ ] Documentation is updated

**Refactoring Status:** ⬜ Not Started | ⬜ In Progress | ⬜ **COMPLETE! 🎉**

---

*Last updated: [Date]*
*Refactored by: [Your Name]*
*Claude Code version: [Version if known]*
