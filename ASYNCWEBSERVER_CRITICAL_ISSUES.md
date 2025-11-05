# AsyncWebServer Critical Issues - URGENT

## Date: 2025-11-05

## Status: CRITICAL - System crashes on API endpoint access

---

## Problem Summary

The AsyncWebServer implementation is causing **Guru Meditation Errors (Core 1 panic)** when ANY API endpoint is accessed via web browser. The device immediately crashes and reboots.

### Error Details
```
Guru Meditation Error: Core 1 panic'ed (LoadProhibited). Exception was unhandled.
EXCCAUSE: 0x0000001c (LoadProhibited - attempted to load from invalid address)
EXCVADDR: 0x0000000e (attempting to access address 0x0000000E - NULL pointer + offset)
```

**This indicates a NULL pointer dereference in the SD mutex or file operations.**

---

## Root Cause Analysis

### Issue 1: FreeRTOS Mutex Not Initialized Before Use

The `g_sdCardMutex` semaphore is created in `initSDMutex()` at [src/main.cpp:698](src/main.cpp#L698), but this happens:
1. **After** WiFi connection
2. **After** SD card initialization
3. **Before** the web server starts

However, the crash backtrace shows the error occurs in `xSemaphoreTake()`, which means:
- The mutex handle might be NULL when AsyncWebServer handlers try to use it
- The semaphore might not be properly created
- There could be a race condition between server initialization and mutex creation

### Issue 2: Incorrect Mutex Usage in AsyncWebServer Handlers

The current implementation uses:
```cpp
#define SD_MUTEX_LOCK()    xSemaphoreTake(g_sdCardMutex, portMAX_DELAY)
#define SD_MUTEX_UNLOCK()  xSemaphoreGive(g_sdCardMutex)
```

**Problems:**
1. `xSemaphoreTake()` can fail if the semaphore handle is NULL
2. No error checking - if `xSemaphoreTake()` returns `pdFALSE`, the code continues anyway
3. `portMAX_DELAY` blocks indefinitely - dangerous in web handlers
4. AsyncWebServer handlers run on different FreeRTOS tasks than where mutex was created

### Issue 3: Semaphore Not Safe for ISR/Async Context

AsyncWebServer uses callbacks that may be called from:
- FreeRTOS task context (OK for mutexes)
- Timer callbacks (NOT safe for mutexes)
- Potentially ISR context (NOT safe for mutexes)

FreeRTOS mutexes (`xSemaphoreCreateMutex()`) **cannot be used from ISRs** and have restrictions in async callbacks.

---

## Immediate Action Required

### Option A: Revert to Critical Sections (NOT RECOMMENDED)

Go back to `portENTER_CRITICAL()` / `portEXIT_CRITICAL()` but this will likely cause the original boot loop issue.

### Option B: Use Binary Semaphore Instead of Mutex

Replace mutex with binary semaphore which is ISR-safe:

**In sd_mutex.h:**
```cpp
extern SemaphoreHandle_t g_sdCardMutex;

void initSDMutex();

// Add NULL check and error handling
inline bool SD_MUTEX_LOCK() {
    if (g_sdCardMutex == NULL) {
        Serial.println("[MUTEX] ERROR: Mutex not initialized!");
        return false;
    }
    if (xSemaphoreTake(g_sdCardMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("[MUTEX] ERROR: Failed to acquire mutex!");
        return false;
    }
    return true;
}

inline void SD_MUTEX_UNLOCK() {
    if (g_sdCardMutex != NULL) {
        xSemaphoreGive(g_sdCardMutex);
    }
}
```

**In sd_mutex.cpp:**
```cpp
#include "sd_mutex.h"

SemaphoreHandle_t g_sdCardMutex = NULL;

void initSDMutex() {
    if (g_sdCardMutex == NULL) {
        // Use binary semaphore instead of mutex - ISR safe
        g_sdCardMutex = xSemaphoreCreateBinary();
        if (g_sdCardMutex != NULL) {
            xSemaphoreGive(g_sdCardMutex);  // Start in "unlocked" state
            Serial.println("[MUTEX] SD mutex initialized successfully");
        } else {
            Serial.println("[MUTEX] FATAL: Failed to create semaphore!");
        }
    }
}
```

**Update all handlers to check return value:**
```cpp
server->on("/api/screens", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!SD_MUTEX_LOCK()) {
        request->send(500, "application/json", "{\"error\":\"Mutex lock failed\"}");
        return;
    }

    // ... do SD operations ...

    SD_MUTEX_UNLOCK();
    request->send(200, "application/json", response);
});
```

### Option C: Disable SD Access in Web Handlers (TEMPORARY WORKAROUND)

Comment out all `SD_MUTEX_LOCK()` / `SD_MUTEX_UNLOCK()` calls and SD operations in web handlers until proper solution is implemented.

---

## Critical Files Affected

1. **[src/webserver/sd_mutex.h](src/webserver/sd_mutex.h)** - Mutex macros need NULL checks
2. **[src/webserver/sd_mutex.cpp](src/webserver/sd_mutex.cpp)** - Mutex initialization
3. **[src/webserver/webserver_manager.cpp](src/webserver/webserver_manager.cpp)** - All API handlers using mutex
4. **[src/main.cpp](src/main.cpp#L698)** - Mutex initialization timing
5. **[src/display/screen_renderer.cpp](src/display/screen_renderer.cpp)** - Uses SD mutex

---

## Test Cases Failing

- ✅ Root page (`/`) - Displays correctly
- ❌ `/api/screens` - **CRASH** (LoadProhibited at 0x0000000E)
- ❌ `/api/files` - **CRASH** (Not tested, but will fail)
- ❌ `/api/download` - **CRASH** (Not tested, but will fail)
- ❌ `/api/disk-usage` - **CRASH** (Not tested, but will fail)
- ❌ `/api/status` - **CRASH** (SD.cardSize() call in handler)
- ✅ `/api/config` - Likely works (no SD access)
- ✅ `/api/sensor-mappings` - Likely works (no SD access)

---

## Why This Wasn't Caught Earlier

1. **No runtime testing** - Code compiled successfully but wasn't tested
2. **Assumption about mutex creation** - Assumed `xSemaphoreCreateMutex()` always succeeds
3. **No error checking** - Macros don't check if mutex is NULL before use
4. **Async context not considered** - Didn't account for AsyncWebServer's callback execution context

---

## Recommended Solution Path

1. **IMMEDIATE**: Add NULL checks and error handling to mutex macros
2. **SHORT TERM**: Replace mutex with binary semaphore (ISR-safe)
3. **VERIFY**: Add logging to confirm mutex creation and usage
4. **TEST**: Test each API endpoint individually
5. **LONG TERM**: Consider queue-based SD access instead of direct locking

---

## Code Changes Required

### Priority 1: Fix Mutex Initialization and Checking

**File: src/webserver/sd_mutex.h**
```cpp
#ifndef SD_MUTEX_H
#define SD_MUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Arduino.h>

extern SemaphoreHandle_t g_sdCardMutex;

// Initialize SD mutex - MUST be called before any SD operations
void initSDMutex();

// Safe mutex locking with NULL check and timeout
inline bool SD_MUTEX_LOCK() {
    if (g_sdCardMutex == NULL) {
        Serial.println("[SD_MUTEX] ERROR: Mutex not initialized!");
        return false;
    }

    // 5 second timeout to avoid deadlock
    if (xSemaphoreTake(g_sdCardMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("[SD_MUTEX] ERROR: Timeout acquiring mutex!");
        return false;
    }

    return true;
}

// Safe mutex unlocking with NULL check
inline void SD_MUTEX_UNLOCK() {
    if (g_sdCardMutex != NULL) {
        xSemaphoreGive(g_sdCardMutex);
    } else {
        Serial.println("[SD_MUTEX] WARNING: Attempted to unlock NULL mutex!");
    }
}

#endif // SD_MUTEX_H
```

**File: src/webserver/sd_mutex.cpp**
```cpp
#include "sd_mutex.h"

SemaphoreHandle_t g_sdCardMutex = NULL;

void initSDMutex() {
    if (g_sdCardMutex != NULL) {
        Serial.println("[SD_MUTEX] WARNING: Mutex already initialized!");
        return;
    }

    // Use binary semaphore - more flexible than mutex for async contexts
    g_sdCardMutex = xSemaphoreCreateBinary();

    if (g_sdCardMutex != NULL) {
        // Give semaphore to start in "unlocked" state
        xSemaphoreGive(g_sdCardMutex);
        Serial.println("[SD_MUTEX] Mutex initialized successfully");
    } else {
        Serial.println("[SD_MUTEX] FATAL ERROR: Failed to create semaphore!");
        // Consider halting or using alternative protection
    }
}
```

### Priority 2: Update All Web Handlers

**Example for /api/screens:**
```cpp
server->on("/api/screens", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!SD_MUTEX_LOCK()) {
        Serial.println("[API] Failed to lock SD mutex for /api/screens");
        request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
        return;
    }

    File screensDir = SD.open("/screens");
    if (!screensDir || !screensDir.isDirectory()) {
        SD_MUTEX_UNLOCK();
        request->send(500, "application/json", "{\"error\":\"Failed to open screens directory\"}");
        return;
    }

    // ... rest of implementation ...

    screensDir.close();
    SD_MUTEX_UNLOCK();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
});
```

**Repeat this pattern for ALL handlers that access SD card.**

---

## Testing Protocol

After implementing fixes:

1. **Serial Monitor**: Watch for mutex initialization message
2. **Test root page**: Confirm web server responds
3. **Test non-SD endpoints**: `/api/config`, `/api/sensor-mappings`
4. **Test SD endpoints ONE AT A TIME**:
   - `/api/status` (SD.cardSize())
   - `/api/disk-usage`
   - `/api/screens`
   - `/api/files`
5. **Check serial output** for mutex errors
6. **Monitor for crashes** after each test

---

## Prevention for Future

1. **Always check return values** from FreeRTOS functions
2. **Add timeouts** to all blocking operations
3. **Test in runtime**, not just compilation
4. **Add debug logging** for critical operations
5. **Consider mutex alternatives** for ISR/async contexts

---

## Status Tracking

- [ ] NULL check added to SD_MUTEX_LOCK()
- [ ] Binary semaphore implemented
- [ ] All web handlers updated with error checking
- [ ] Mutex initialization verified with logging
- [ ] Each API endpoint tested individually
- [ ] System runs stable for 10+ minutes without crash
- [ ] Multiple simultaneous API calls tested
- [ ] Boot loop issue still resolved

---

## Contact/Notes

**Issue discovered**: 2025-11-05
**Severity**: CRITICAL - System unusable
**Priority**: P0 - Fix immediately

The AsyncWebServer migration cannot be considered complete until this is resolved. The device crashes on ANY SD-related API call.
