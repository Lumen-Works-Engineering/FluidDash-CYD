# FluidDash-CYD Stability Analysis - Lessons from FluidDial

**Date:** 2025-11-04
**Analysis:** Comparison of FluidDial (stable) vs FluidDash-CYD (crashing)

---

## Executive Summary

After analyzing the stable **FluidDial** project, several critical differences explain why it runs reliably on the CYD hardware while **FluidDash-CYD** experiences crashes. The primary issues are:

1. **No FreeRTOS background tasks** in FluidDial
2. **No AsyncWebServer** complexity
3. **Simple, single-threaded event loop**
4. **Minimal dynamic memory allocation**
5. **Straightforward initialization pattern**

---

## Key Architectural Differences

### 1. Task Management

#### FluidDial Pattern ✅
- **Zero FreeRTOS tasks** - completely single-threaded
- Simple event loop in `loop()`:
  ```cpp
  void loop() {
      fnc_poll();         // Handle messages from FluidNC
      dispatch_events();  // Handle dial, touch, buttons
  }
  ```
- No mutex complexity
- No task synchronization issues
- No watchdog complications

#### FluidDash-CYD Pattern ⚠️
- AsyncWebServer creates background tasks automatically
- WebSocket client with complex retry logic
- Mutex protection for SD card access
- Watchdog timer feeding throughout code
- Multiple concurrent operations

**Impact:** Background tasks are the #1 source of crashes on ESP32 when not carefully managed.

---

### 2. Initialization Simplicity

#### FluidDial Setup (ardmain.cpp:16-34) ✅
```cpp
void setup() {
    init_system();                    // Hardware init
    display.setBrightness(...);
    show_logo();
    delay_ms(2000);                   // Simple delay
    base_display();
    fnc_realtime(StatusReport);
    activate_scene(initMenus());      // Start UI
}
```

**Total: ~10 lines of code, 4 steps**

#### FluidDash-CYD Setup (main.cpp:600-825) ⚠️
- Phase 0: Mutex initialization + verification
- Phase 1: Display + RTC + ADC + PWM + sensors
- Phase 2: SD card with complex error handling
- Phase 3: WiFi connection with retry logic
- Phase 4: Web server startup
- Watchdog feeding between every operation
- Complex state verification

**Total: ~225 lines of code, 20+ steps**

**Impact:** Complex initialization increases crash risk during startup.

---

### 3. Memory Management

#### FluidDial Memory Pattern ✅
- **Fixed-size sprite** created once at startup:
  ```cpp
  canvas.setColorDepth(8);
  canvas.createSprite(240, 240);  // Never deleted
  ```
- Color depth: 8-bit (memory efficient)
- Sprites created once, reused forever
- No dynamic allocation in loop
- Small UART buffer (256 bytes)

#### FluidDash-CYD Memory Pattern ⚠️
```cpp
// Dynamic allocation
float *tempHistory = nullptr;

// Large String operations (heap fragmentation!)
String getMainHTML() {
    String html = String(FPSTR(MAIN_HTML));  // ~8KB copy
    html.replace("%DEVICE_NAME%", cfg.device_name);  // More allocations
    html.replace("%IP_ADDRESS%", ...);
    return html;  // Copy on return
}
```

**Issues:**
- Dynamic memory allocation during runtime
- Large String concatenations (causes heap fragmentation)
- AsyncWebServer buffers (automatic allocation)
- WebSocket buffers
- History buffer resizing

**Impact:** Heap fragmentation leads to crashes after hours of operation.

---

### 4. Network Communication

#### FluidDial Network ✅
- Simple UART communication to FluidNC
- Direct hardware UART driver (no tasks)
- Software flow control (XON/XOFF)
- Fixed 256-byte buffer
- Polled in main loop with `fnc_poll()`

#### FluidDash-CYD Network ⚠️
- AsyncWebServer (creates background tasks)
- WebSocket client (persistent connection)
- WiFiManager (complex state machine)
- HTTP request handlers
- JSON parsing/generation
- mDNS responder

**Impact:** Each network library adds complexity and potential crash points.

---

### 5. Display Updates

#### FluidDial Display ✅
- All drawing to offscreen canvas
- Single `canvas.pushSprite()` to display
- Scene-based state machine
- Synchronous rendering
- No display access from interrupts

#### FluidDash-CYD Display ⚠️
- Direct display writing in some cases
- Updates from multiple code paths
- Potential concurrent access (display + web server)
- Complex JSON-based layout rendering

**Impact:** Concurrent display access can cause SPI bus conflicts.

---

## Critical Crash Sources in FluidDash-CYD

### 1. AsyncWebServer (HIGH PRIORITY)

**Why it crashes:**
- Creates hidden FreeRTOS tasks
- Uses interrupts for TCP/IP stack
- Allocates memory in background
- Known mutex issues (see: `ASYNCWEBSERVER_CRITICAL_ISSUES.md`)

**Evidence:**
```cpp
// From your existing documentation
ASYNCWEBSERVER_CRITICAL_ISSUES.md
ASYNCWEBSERVER_MUTEX_FIX.md
ASYNCWEBSERVER_MUTEX_FIX_V2.md
```

**Solution:**
- Replace with synchronous ESP32WebServer
- Or thoroughly test AsyncWebServer with proper mutex protection
- Or disable web server until core is stable

---

### 2. WebSocket Client (HIGH PRIORITY)

**Current complexity:**
```cpp
// From main.cpp:858-914
static unsigned long lastWebSocketLoop = 0;
static unsigned long connectionAttemptStart = 0;
static unsigned long lastConnectionAttempt = 0;
static bool attemptingConnection = false;

// Throttle webSocket.loop() calls to prevent blocking
if (millis() - lastWebSocketLoop >= 100) {
    // Track if we're in a connection attempt that might block
    if (!fluidncConnected && !attemptingConnection) {
        connectionAttemptStart = millis();
        attemptingConnection = true;
    }
    webSocket.loop();
    // ... 30+ more lines of retry logic
}
```

**Issues:**
- Connection attempts can block
- Complex retry state machine
- Runs in main loop but WebSocket library may spawn tasks
- Timeout handling is fragile

**Solution:**
- Use simple HTTP polling instead of persistent WebSocket
- Or use a simpler WebSocket library
- Or connect only when needed, disconnect when idle

---

### 3. Dynamic Memory Allocation (MEDIUM PRIORITY)

**Problems:**
```cpp
// Heap fragmentation sources
float *tempHistory = nullptr;  // Resized on config change
String html = String(FPSTR(...));  // 8KB+ allocations
AsyncWebServerResponse  // Unknown size allocations
```

**Solution:**
```cpp
// Pre-allocate maximum sizes
#define MAX_HISTORY 3600
float tempHistory[MAX_HISTORY];
uint16_t historySize = 0;  // Actual used size

// Use streaming responses instead of String
void handleMainPage(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(FPSTR(MAIN_HTML_PART1));
    response->printf("<span>%s</span>", cfg.device_name);
    response->print(FPSTR(MAIN_HTML_PART2));
    request->send(response);
}
```

---

### 4. Complex Initialization (MEDIUM PRIORITY)

**Current approach:**
- 4 phases with verification between each
- Watchdog feeding throughout
- Delays during setup
- Mutex verification with halt-on-failure

**Recommended approach:**
```cpp
void setup() {
    Serial.begin(115200);

    // Core hardware only
    gfx.init();
    gfx.setRotation(1);
    showSplashScreen();

    pinMode(BTN_MODE, INPUT_PULLUP);
    pinMode(FAN_PWM, OUTPUT);

    // Defer complex init to loop
    delay(1000);
}

void loop() {
    static uint8_t initPhase = 0;

    if (initPhase == 0) {
        // First loop: initialize sensors
        initSensors();
        initPhase++;
    } else if (initPhase == 1 && millis() > 3000) {
        // After 3 seconds: try WiFi
        initWiFi();
        initPhase++;
    } else if (initPhase == 2 && WiFi.status() == WL_CONNECTED) {
        // When WiFi ready: start web server
        initWebServer();
        initPhase++;
    }

    // Normal operation
    if (initPhase >= 3) {
        updateSensors();
        updateDisplay();
    }

    yield();
}
```

---

### 5. Platform Version Mismatch (LOW PRIORITY)

**FluidDial (platformio.ini:32):**
```ini
platform_packages = platformio/framework-arduinoespressif32@3.20017.0
```

**FluidDash-CYD (platformio.ini:12):**
```ini
platform = espressif32@^6.8.0
```

**Differences:**
- Different Arduino core versions
- Different FreeRTOS configuration defaults
- Different WiFi stack behavior
- Different memory allocator

**Recommendation:**
- Test with espressif32@3.20017.0 to match FluidDial
- Or carefully tune FreeRTOS settings for 6.8.0

---

## Immediate Action Plan

### Step 1: Isolate Crash Source (Testing)

Test each component separately:

```cpp
// Test 1: Disable AsyncWebServer
void setup() {
    // ... init code ...
    // webServer.begin();  // COMMENT OUT
}
```

**Expected result:** If crashes stop, AsyncWebServer is the culprit.

```cpp
// Test 2: Disable WebSocket
void loop() {
    // webSocket.loop();  // COMMENT OUT
    // if (fluidncConnected && ...) { webSocket.sendTXT("?"); }  // COMMENT OUT
}
```

**Expected result:** If crashes stop, WebSocket is the culprit.

```cpp
// Test 3: Disable dynamic allocation
// Replace:
float *tempHistory = nullptr;
// With:
float tempHistory[600];  // Fixed size
uint16_t historySize = 600;
// Comment out: allocateHistoryBuffer();
```

**Expected result:** If crashes stop, heap fragmentation is the culprit.

---

### Step 2: Simplify Core Loop (Implementation)

**Target pattern:**
```cpp
void loop() {
    // 1. Update sensors (fast)
    static unsigned long lastSensor = 0;
    if (millis() - lastSensor >= 100) {
        updateSensorsNonBlocking();
        lastSensor = millis();
    }

    // 2. Update display (1 Hz)
    static unsigned long lastDisplay = 0;
    if (millis() - lastDisplay >= 1000) {
        updateDisplay();
        lastDisplay = millis();
    }

    // 3. Handle button (fast)
    handleButton();

    // 4. Optional: Web server (if using sync version)
    // server.handleClient();

    // 5. Yield to prevent watchdog
    yield();
}
```

**Remove from loop:**
- WebSocket connection logic
- Watchdog feeding (rely on yield() instead)
- Complex retry state machines

---

### Step 3: Replace AsyncWebServer (Critical)

**Option A: Use ESP32WebServer (Recommended)**

```cpp
#include <WebServer.h>  // Instead of ESPAsyncWebServer

WebServer server(80);

void setup() {
    // ... other init ...

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", FPSTR(MAIN_HTML));
    });

    server.on("/api/status", HTTP_GET, []() {
        server.send(200, "application/json", getStatusJSON());
    });

    server.begin();
}

void loop() {
    server.handleClient();  // Non-blocking, must call frequently
    // ... rest of loop ...
}
```

**Benefits:**
- No background tasks
- No mutex complexity
- Proven stable
- Synchronous operation

**Option B: Fix AsyncWebServer**

If you must use AsyncWebServer:
```cpp
// 1. Create all mutexes BEFORE any tasks
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t sdMutex;

void setup() {
    displayMutex = xSemaphoreCreateMutex();
    sdMutex = xSemaphoreCreateMutex();

    // 2. Protect ALL shared resources
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Access SD card
            xSemaphoreGive(sdMutex);
        }
    });

    // 3. Limit concurrent connections
    AsyncWebServer server(80);
    server.setMaxClients(2);  // Reduce memory pressure
}
```

---

### Step 4: Simplify WebSocket or Remove

**Option A: Remove WebSocket, use HTTP polling**

```cpp
void loop() {
    static unsigned long lastPoll = 0;

    if (WiFi.status() == WL_CONNECTED && millis() - lastPoll >= 2000) {
        HTTPClient http;
        http.begin("http://" + String(cfg.fluidnc_ip) + "/api/status");
        int httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();
            parseFluidNCStatus(payload);  // Parse JSON
        }

        http.end();
        lastPoll = millis();
    }
}
```

**Benefits:**
- No persistent connection
- Simpler error handling
- No background tasks
- Easy to throttle

**Option B: Simplify WebSocket**

```cpp
void loop() {
    static unsigned long lastWSLoop = 0;

    // Only call webSocket.loop() every 500ms
    if (millis() - lastWSLoop >= 500) {
        webSocket.loop();
        lastWSLoop = millis();
    }

    // Remove all retry logic - let library handle it
}
```

---

### Step 5: Pre-Allocate All Memory

**Current (dynamic):**
```cpp
float *tempHistory = nullptr;

void allocateHistoryBuffer() {
    if (tempHistory != nullptr) {
        delete[] tempHistory;
    }
    historySize = cfg.graph_timespan_seconds / cfg.graph_update_interval;
    tempHistory = new float[historySize];
}
```

**Fixed (static):**
```cpp
#define MAX_HISTORY_SIZE 3600  // 1 hour at 1 second intervals
float tempHistory[MAX_HISTORY_SIZE];
uint16_t historySize = 0;

void allocateHistoryBuffer() {
    historySize = min(
        (uint16_t)(cfg.graph_timespan_seconds / cfg.graph_update_interval),
        MAX_HISTORY_SIZE
    );
    // No allocation needed - buffer is static
}
```

**Apply to all buffers:**
- Temperature history
- String buffers
- JSON buffers
- Display buffers

---

## Long-Term Recommendations

### 1. Adopt FluidDial's Architecture

Consider restructuring FluidDash-CYD to match FluidDial's proven pattern:

**Core principles:**
- Single-threaded event loop
- Scene-based state machine
- Fixed memory allocation
- Polled I/O (no interrupts for UI)
- Simple initialization

### 2. Memory Monitoring

Add heap monitoring to detect leaks:

```cpp
void loop() {
    static unsigned long lastMemCheck = 0;

    if (millis() - lastMemCheck >= 10000) {
        Serial.printf("[MEM] Free: %d bytes, Min: %d bytes\n",
            ESP.getFreeHeap(),
            ESP.getMinFreeHeap());
        lastMemCheck = millis();
    }
}
```

Watch for:
- Decreasing free heap over time
- Minimum free heap below 20KB

### 3. Watchdog Strategy

**Current:** Feed watchdog throughout code
**Recommended:** Rely on `yield()` in main loop

```cpp
void loop() {
    // Do work
    updateSensors();
    updateDisplay();

    // Single yield at end
    yield();  // Feeds watchdog automatically
}
```

Only use explicit watchdog feeding if a single operation takes >1 second.

### 4. Error Recovery

Add graceful degradation:

```cpp
void loop() {
    static uint8_t crashCount = 0;

    if (ESP.getResetReason() == "Software Watchdog") {
        crashCount++;

        if (crashCount >= 3) {
            // Disable problematic features
            disableWebServer();
            disableWebSocket();
        }
    }

    // ... normal operation ...
}
```

### 5. Configuration Simplification

Reduce configuration complexity:
- Remove dynamic history buffer sizing
- Use fixed, tested values
- Defer non-critical features

---

## Testing Checklist

Before deploying changes:

- [ ] Run for 24 hours without crashes
- [ ] Monitor heap usage (must stay stable)
- [ ] Test all web endpoints
- [ ] Test FluidNC connection/disconnection
- [ ] Test WiFi reconnection
- [ ] Test SD card removal/insertion
- [ ] Verify display updates continue smoothly
- [ ] Check sensor readings stay accurate

---

## Code Examples: Before and After

### Example 1: Web Server

**Before (AsyncWebServer):**
```cpp
AsyncWebServer server(80);

server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = getStatusJSON();  // 2KB allocation
    request->send(200, "application/json", json);
});
```

**After (ESP32WebServer):**
```cpp
WebServer server(80);

server.on("/api/status", HTTP_GET, []() {
    server.send(200, "application/json", getStatusJSON());
});

void loop() {
    server.handleClient();
}
```

### Example 2: Memory Allocation

**Before:**
```cpp
String getMainHTML() {
    String html = String(FPSTR(MAIN_HTML));
    html.replace("%DEVICE_NAME%", cfg.device_name);
    return html;
}
```

**After:**
```cpp
void handleMainHTML(WebServer &server) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // Stream in chunks
    server.sendContent(FPSTR(MAIN_HTML_PART1));
    server.sendContent(cfg.device_name);
    server.sendContent(FPSTR(MAIN_HTML_PART2));
}
```

### Example 3: Initialization

**Before:**
```cpp
void setup() {
    initSDMutex();
    feedLoopWDT();
    initDefaultConfig();
    feedLoopWDT();
    enableLoopWDT();
    feedLoopWDT();
    // ... 200 more lines ...
}
```

**After:**
```cpp
void setup() {
    Serial.begin(115200);
    gfx.init();
    showSplashScreen();
    initSensors();
    delay(1000);
}

void loop() {
    static bool initialized = false;
    if (!initialized) {
        initialized = initNetwork();  // Returns true when done
    }

    updateSensors();
    updateDisplay();
    yield();
}
```

---

## Conclusion

FluidDial's stability comes from its **simplicity**:
- No background tasks
- No complex threading
- Fixed memory allocation
- Simple event loop
- Minimal dependencies

FluidDash-CYD can achieve the same stability by:
1. **Removing AsyncWebServer** (use ESP32WebServer)
2. **Simplifying WebSocket** (or replace with HTTP polling)
3. **Pre-allocating memory** (no dynamic allocation in loop)
4. **Simplifying initialization** (defer complex init to loop)
5. **Following FluidDial's patterns** (proven on same hardware)

The key insight: **Complexity is the enemy of stability on embedded systems.**

Start with the simplest possible implementation that works, then add features incrementally while monitoring for crashes.

---

## References

- FluidDial source: `C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDial\`
  - Main loop: `src/ardmain.cpp:36-39`
  - Initialization: `src/SystemArduino.cpp:118-129`
  - Hardware setup: `src/Hardware2432.cpp:425-462`
  - Event handling: `src/Scene.cpp:153-189`

- FluidDash-CYD existing issues:
  - `ASYNCWEBSERVER_CRITICAL_ISSUES.md`
  - `ASYNCWEBSERVER_MUTEX_FIX.md`
  - `ASYNCWEBSERVER_MUTEX_FIX_V2.md`

- Platform documentation:
  - ESP32 Arduino Core: https://docs.espressif.com/projects/arduino-esp32/
  - PlatformIO ESP32: https://docs.platformio.org/en/latest/platforms/espressif32.html
