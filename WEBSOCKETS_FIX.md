# WebSockets Library Integration - Technical Documentation

## Project: FluidDash (formerly CNC Guardian)
**Date:** October 22, 2025
**Platform:** ESP32 (ESP32-D0WD-V3)
**Framework:** Arduino (espressif32@^6.8.0)

---

## Overview

This document details the resolution of WebSockets library compilation and linking issues encountered when integrating the `links2004/WebSockets@2.7.1` library into the FluidDash project for real-time communication with FluidNC CNC controller.

---

## Problem Statement

### Initial Issue
The WebSockets library failed to compile with the following error:
```
fatal error: WiFi.h: No such file or directory
 #include <WiFi.h>
          ^~~~~~~~
```

### Root Cause
PlatformIO's Library Dependency Finder (LDF) compiles libraries in isolation. When the WebSockets library was compiled, it did not have access to the ESP32 Arduino framework headers (`WiFi.h` and `WiFiClientSecure.h`), even though these are built-in framework libraries.

### Secondary Issue
After initial compilation fixes, a linker error occurred:
```
undefined reference to `WiFiClientSecure::setInsecure()'
```

This happened because the WiFiClientSecure library implementation was not being linked into the final firmware.

---

## Solution

The solution required two complementary fixes:

### 1. Add Framework Library Include Paths (platformio.ini)

**File:** `platformio.ini`
**Lines:** 22-25

```ini
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=0
    -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFiClientSecure/src
    -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFi/src
```

**Why This Works:**
- Adds explicit include paths to the ESP32 framework's WiFi libraries
- Makes WiFi.h and WiFiClientSecure.h headers available during library compilation
- The `$PROJECT_PACKAGES_DIR` variable expands to the PlatformIO packages directory
- These flags apply to ALL compilation units (source files and libraries)

### 2. Include WiFiClientSecure in Main Source (src/main.cpp)

**File:** `src/main.cpp`
**Line:** 14

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>  // Added for WebSockets SSL support
#include <WiFiManager.h>
```

**Why This Works:**
- Forces PlatformIO's LDF to discover and include the WiFiClientSecure library
- Ensures the WiFiClientSecure implementation is compiled and linked
- Resolves the "undefined reference" linker errors
- The LDF uses a "deep+" mode to scan for dependencies

---

## Build Configuration Details

### Complete platformio.ini Configuration

```ini
[env:esp32dev]
platform = espressif32@^6.8.0
board = esp32dev
framework = arduino
board_build.flash_size = 4MB
monitor_speed = 115200
upload_port = COM7
monitor_port = COM7
lib_ldf_mode = deep+
lib_compat_mode = soft
lib_ignore = AsyncTCP_RP2040W
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=0
    -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFiClientSecure/src
    -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFi/src
lib_deps =
	adafruit/RTClib@^2.1.4
	moononournation/GFX Library for Arduino@1.3.7
	https://github.com/me-no-dev/ESPAsyncWebServer.git
	https://github.com/me-no-dev/AsyncTCP.git
	https://github.com/tzapu/WiFiManager.git
	links2004/WebSockets@2.7.1
```

### Key Configuration Options

- **lib_ldf_mode = deep+**: Enables deep dependency scanning including framework libraries
- **lib_compat_mode = soft**: Allows more flexible library compatibility checks
- **lib_ignore = AsyncTCP_RP2040W**: Prevents Raspberry Pi library from being included

---

## Dependency Graph (Final)

Successfully resolved dependency tree:

```
|-- RTClib @ 2.1.4
|-- GFX Library for Arduino @ 1.3.7
|-- ESPAsyncWebServer @ 3.6.0+sha.ad3741d
|-- AsyncTCP @ 3.3.2+sha.ef448a8
|-- WiFiManager @ 2.0.17+sha.32655b7
|-- WebSockets @ 2.7.1                    ← Target library
|-- SPI @ 2.0.0
|-- Wire @ 2.0.0
|-- Adafruit BusIO @ 1.17.4
|-- WiFi @ 2.0.0
|-- WiFiClientSecure @ 2.0.0              ← Required for linking
|-- Update @ 2.0.0
|-- WebServer @ 2.0.0
|-- DNSServer @ 2.0.0
|-- Preferences @ 2.0.0
|-- FS @ 2.0.0
|-- ESPmDNS @ 2.0.0
```

---

## Build Results

### Memory Usage
- **RAM:** 15.6% (51,096 bytes of 327,680 bytes)
- **Flash:** 85.6% (1,121,345 bytes of 1,310,720 bytes)

### Upload Success
```
Serial port COM7
Chip is ESP32-D0WD-V3 (revision v3.1)
Features: WiFi, BT, Dual Core, 240MHz
MAC: 14:08:08:a5:f4:94
```

### Runtime Verification
✅ Device successfully connects to FluidNC at `ws://192.168.73.13:81/ws`

---

## Troubleshooting History

### Attempts That Did NOT Work

1. **Adding WiFi to lib_deps**
   ```ini
   lib_deps = WiFi
   ```
   - Failed: WiFi is a framework library, not a registry library

2. **Adding WiFiClientSecure to lib_deps**
   ```ini
   lib_deps = WiFiClientSecure
   ```
   - Failed: Same issue as WiFi

3. **Using build_src_flags instead of build_flags**
   ```ini
   build_src_flags =
       -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFi/src
   ```
   - Failed: Only applies to source files, not library compilation

4. **Changing lib_compat_mode to strict**
   - Failed: Too restrictive, caused other compatibility issues

5. **Using GitHub URL for WebSockets**
   ```ini
   lib_deps = https://github.com/Links2004/arduinoWebSockets.git
   ```
   - Failed: Same compilation issues

6. **Removing WebSockets library entirely**
   - Success: Build worked, but defeated the purpose (no WebSocket functionality)

---

## FluidNC WebSocket Integration

### Connection Details
- **Protocol:** WebSocket (ws://)
- **Host:** 192.168.73.13 (FluidNC controller)
- **Port:** 81 (FluidNC WebSocket default)
- **Path:** /ws
- **Full URI:** `ws://192.168.73.13:81/ws`

### Implementation in main.cpp

```cpp
void connectFluidNC() {
  Serial.printf("[FluidNC] Attempting to connect to ws://%s:%d/ws\n",
                cfg.fluidnc_ip, cfg.fluidnc_port);
  webSocket.begin(cfg.fluidnc_ip, cfg.fluidnc_port, "/ws");
  webSocket.onEvent(fluidNCWebSocketEvent);
  webSocket.setReconnectInterval(5000);
  Serial.println("[FluidNC] WebSocket initialized, waiting for connection...");
}
```

### WebSocket Event Handler

```cpp
void fluidNCWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[FluidNC] Disconnected");
      fluidncConnected = false;
      break;
    case WStype_CONNECTED:
      Serial.printf("[FluidNC] Connected to %s\n", (char*)payload);
      fluidncConnected = true;
      break;
    case WStype_TEXT:
      Serial.printf("[FluidNC] Received: %s\n", (char*)payload);
      parseFluidNCStatus((char*)payload);
      break;
    case WStype_ERROR:
      Serial.println("[FluidNC] WebSocket Error");
      fluidncConnected = false;
      break;
  }
}
```

---

## Known Warnings (Non-Critical)

### GFX Library SPI_MAX_PIXELS_AT_ONCE Redefinition
```
warning: "SPI_MAX_PIXELS_AT_ONCE" redefined
```
- **Impact:** None - cosmetic warning only
- **Cause:** GFX library has multiple display drivers with different buffer sizes
- **Action:** Can be safely ignored

### Arduino_ESP32QSPI Narrowing Conversion
```
warning: narrowing conversion of '_dataMode' from 'int8_t' to 'uint8_t'
```
- **Impact:** None - GFX library internal issue
- **Cause:** Type conversion in QSPI initialization
- **Action:** Can be safely ignored

---

## Related Files Modified

1. **platformio.ini** - Build configuration and library dependencies
2. **src/main.cpp** - Added WiFiClientSecure include

## Related Files NOT Modified

- All WebSocket implementation code was already present
- No changes to FluidNC connection logic were needed
- Only build system configuration was required

---

## Future Considerations

### If WebSockets Library Updates Break Build

1. Check if framework library paths have changed
2. Verify WiFiClientSecure is still at the same location
3. Consider pinning WebSockets library to exact version: `links2004/WebSockets@2.7.1`
4. Review PlatformIO release notes for ESP32 framework changes

### Alternative Approaches (Not Recommended)

1. **Manual library installation**: Copy WebSockets to lib/ folder
   - Pros: Full control over library
   - Cons: Harder to update, not tracked by PlatformIO

2. **Use ArduinoWebsockets library instead**: `gilmaimon/ArduinoWebsockets`
   - Pros: Different implementation, might avoid issues
   - Cons: Different API, requires code changes

3. **Use ESP-IDF WebSocket client**: Native ESP-IDF implementation
   - Pros: Official Espressif support
   - Cons: Would require switching from Arduino framework

---

## Success Criteria Met

✅ WebSockets library compiles without errors
✅ All dependencies resolved correctly
✅ Firmware links successfully
✅ Upload to ESP32 successful
✅ WebSocket connection to FluidNC established
✅ Real-time communication with CNC controller working
✅ Memory usage within acceptable limits

---

## Additional Resources

- **WebSockets Library:** https://github.com/Links2004/arduinoWebSockets
- **PlatformIO LDF Documentation:** https://docs.platformio.org/en/latest/librarymanager/ldf.html
- **ESP32 Arduino Framework:** https://github.com/espressif/arduino-esp32
- **FluidNC WebSocket API:** https://github.com/bdring/FluidNC

---

## Maintenance Notes

### When to Review This Document

- Before upgrading ESP32 Arduino framework
- Before upgrading WebSockets library
- When adding new WebSocket features
- If compilation errors reappear after PlatformIO updates

### Git Commit Reference

When committing these changes, use:
```bash
git add platformio.ini src/main.cpp
git commit -m "Fix WebSockets library compilation - add WiFiClientSecure support

- Added framework library include paths in platformio.ini for WiFi headers
- Added WiFiClientSecure include in main.cpp for proper linking
- WebSockets @ 2.7.1 now compiles and links successfully
- FluidNC WebSocket connection working at ws://192.168.73.13:81/ws
- See WEBSOCKETS_FIX.md for detailed documentation"
```

---

**Document Created:** October 22, 2025
**Last Updated:** October 22, 2025
**Author:** Development session with Claude Code
**Status:** ✅ RESOLVED - Working in production
