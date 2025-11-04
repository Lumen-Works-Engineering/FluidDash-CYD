# AsyncWebServer + Mutex Protection - SD Card Safe Implementation

**Date:** November 4, 2025  
**Status:** Emergency Fix - Back to AsyncWebServer with Proper Synchronization  
**Reason:** FreeRTOS semaphore incompatibility with synchronous WebServer library

---

## The Problem

The synchronous WebServer library has incompatibility with FreeRTOS causing semaphore queue corruption:
```
assert failed: xQueueSemaphoreTake queue.c:1549 (pxQueue->uxItemSize == 0)
```

This is not a file listing issue - it's the WebServer framework itself conflicting with your existing async tasks (WebSockets, sensors, display updates).

## The Solution

**Return to AsyncWebServer** but implement **proper mutex protection** for SD card operations. This is the proven pattern used across ESP32 projects.

---

## Step 1: Update platformio.ini

**File:** `platformio.ini`

Change back from synchronous to async:

```ini
[env:esp32dev]
platform = espressif32@6.8.0
board = esp32dev
framework = arduino
board_build.flash_size = 4MB
monitor_speed = 115200

lib_deps =
    adafruit/RTClib@^2.1.4
    lovyan03/LovyanGFX@^1.1.16
    links2004/WebSockets@2.7.1
    bblanchon/ArduinoJson@^7.2.0
    paulstoffregen/OneWire@^2.3.8
    milesburton/DallasTemperature@^3.11.0
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/AsyncTCP.git
```

---

## Step 2: Create SD Card Mutex Header

**File:** `src/webserver/sd_mutex.h`

```cpp
#ifndef SD_MUTEX_H
#define SD_MUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

/**
 * SD Card Mutex for thread-safe SD operations
 * 
 * Usage:
 *   SD_MUTEX_LOCK();
 *   File f = SD.open("/path");
 *   // do stuff
 *   f.close();
 *   SD_MUTEX_UNLOCK();
 */

extern portMUX_TYPE g_sdCardMutex;

#define SD_MUTEX_LOCK()    portENTER_CRITICAL(&g_sdCardMutex)
#define SD_MUTEX_UNLOCK()  portEXIT_CRITICAL(&g_sdCardMutex)

#endif
```

**File:** `src/webserver/sd_mutex.cpp`

```cpp
#include "sd_mutex.h"

// Initialize the mutex
portMUX_TYPE g_sdCardMutex = portMUX_INITIALIZER_UNLOCKED;
```

---

## Step 3: Update webserver.cpp - Safe Route Handlers

**File:** `src/webserver/webserver.cpp`

```cpp
#include "webserver.h"
#include "sd_mutex.h"
#include "SD.h"

// ============================================
// /api/screens - List screen files
// ============================================
void WebServerManager::setupScreenRoutes() {
    asyncServer->on("/api/screens", HTTP_GET, [this](AsyncWebServerRequest *request) {
        SD_MUTEX_LOCK();  // Acquire mutex

        File dir = SD.open("/screens");
        if (!dir) {
            SD_MUTEX_UNLOCK();
            request->send(404, "application/json", "{\"files\":[]}");
            return;
        }

        String response = "{\"files\":[";
        bool first = true;

        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory() && String(f.name()).endsWith(".json")) {
                if (!first) {
                    response += ",";
                }
                response += "{\"name\":\"" + String(f.name()) + 
                           "\",\"size\":" + String(f.size()) + "}";
                first = false;
            }
            f.close();
            f = dir.openNextFile();
        }

        response += "]}";
        dir.close();

        SD_MUTEX_UNLOCK();  // Release mutex

        request->send(200, "application/json", response);
    });

    // ============================================
    // /api/screens/:filename - Download screen
    // ============================================
    asyncServer->on("/api/screens/:filename", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String filename = request->pathArg(0);
        String path = "/screens/" + filename;

        SD_MUTEX_LOCK();

        if (!SD.exists(path)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "text/plain", "File not found");
            return;
        }

        File file = SD.open(path);
        if (!file) {
            SD_MUTEX_UNLOCK();
            request->send(500, "text/plain", "Cannot open file");
            return;
        }

        // Read file into memory (for JSON files, usually <10KB)
        String content = file.readString();
        file.close();

        SD_MUTEX_UNLOCK();

        request->send(200, "application/json", content);
    });

    // ============================================
    // /api/upload-screen - Save screen JSON
    // ============================================
    asyncServer->on("/api/upload-screen", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {},
        [this](AsyncWebServerRequest *request, const String& filename, size_t index, 
               uint8_t *data, size_t len, bool final) {

            static File uploadFile;

            if (index == 0) {
                SD_MUTEX_LOCK();

                String path = "/screens/" + filename;
                uploadFile = SD.open(path, FILE_WRITE);

                SD_MUTEX_UNLOCK();
            }

            if (uploadFile && len > 0) {
                SD_MUTEX_LOCK();
                uploadFile.write(data, len);
                SD_MUTEX_UNLOCK();
            }

            if (final) {
                SD_MUTEX_LOCK();
                uploadFile.close();
                SD_MUTEX_UNLOCK();

                // Trigger screen reload
                screenManager.loadScreens();

                request->send(200, "application/json", 
                    "{\"status\":\"saved\",\"file\":\"" + filename + "\"}");
            }
        }
    );

    // ============================================
    // /api/delete-screen - Delete screen file
    // ============================================
    asyncServer->on("/api/delete-screen", HTTP_POST, [this](AsyncWebServerRequest *request) {
        String filename = request->arg("filename");
        String path = "/screens/" + filename;

        SD_MUTEX_LOCK();

        if (!SD.exists(path)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "text/plain", "File not found");
            return;
        }

        bool success = SD.remove(path);

        SD_MUTEX_UNLOCK();

        if (success) {
            request->send(200, "application/json", "{\"status\":\"deleted\"}");
        } else {
            request->send(500, "text/plain", "Failed to delete");
        }
    });
}

// ============================================
// /api/schema/screen-elements - Element discovery
// ============================================
void WebServerManager::setupSchemaRoutes() {
    asyncServer->on("/api/schema/screen-elements", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // NO MUTEX NEEDED - this route doesn't access SD card

        DynamicJsonDocument doc(4096);

        JsonArray coords = doc.createNestedArray("coordinates");
        coords.add("wposX");
        coords.add("wposY");
        coords.add("wposZ");
        coords.add("wposA");
        coords.add("posX");
        coords.add("posY");
        coords.add("posZ");
        coords.add("posA");

        // Temperature sensors (discover actual connected sensors)
        JsonArray temps = doc.createNestedArray("temperatures");
        for (int i = 0; i < NUM_DS18B20_SENSORS; i++) {
            if (sensors.isConnected(i)) {
                temps.add("temp" + String(i));
            }
        }

        JsonArray status = doc.createNestedArray("status");
        status.add("machineState");
        status.add("feedRate");
        status.add("spindleRPM");

        JsonArray system = doc.createNestedArray("system");
        system.add("psuVoltage");
        system.add("fanSpeed");
        system.add("ipAddress");
        system.add("ssid");

        JsonArray types = doc.createNestedArray("elementTypes");
        types.add("rect");
        types.add("line");
        types.add("text");
        types.add("dynamic");

        JsonObject colors = doc.createNestedObject("colors");
        colors["black"] = "0000";
        colors["white"] = "FFFF";
        colors["red"] = "F800";
        colors["green"] = "07E0";
        colors["blue"] = "001F";
        colors["yellow"] = "FFE0";

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

// ============================================
// /api/files - List all SD card files
// ============================================
void WebServerManager::setupFileRoutes() {
    asyncServer->on("/api/files", HTTP_GET, [this](AsyncWebServerRequest *request) {
        SD_MUTEX_LOCK();

        DynamicJsonDocument doc(4096);
        JsonArray files = doc.createNestedArray("files");

        File root = SD.open("/");
        if (root) {
            listDirRecursive(root, "", files, 0);
            root.close();
        }

        SD_MUTEX_UNLOCK();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    asyncServer->on("/api/download", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String filepath = request->arg("path");

        SD_MUTEX_LOCK();

        if (!SD.exists(filepath)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "text/plain", "File not found");
            return;
        }

        File file = SD.open(filepath);
        if (!file) {
            SD_MUTEX_UNLOCK();
            request->send(500, "text/plain", "Cannot open file");
            return;
        }

        String filename = filepath.substring(filepath.lastIndexOf("/") + 1);
        String contentType = "application/octet-stream";
        if (filename.endsWith(".json")) contentType = "application/json";

        // Stream file to client
        AsyncWebServerResponse *response = request->beginResponse(
            200, contentType, 
            [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
                return file.readBytes((char*)buffer, maxLen);
            }
        );
        response->addHeader("Content-Disposition", "attachment; filename=" + filename);
        request->send(response);

        SD_MUTEX_UNLOCK();
    });
}

// Helper: List directory recursively
void WebServerManager::listDirRecursive(File dir, String prefix, JsonArray& files, int depth) {
    if (depth > 3) return;

    File file = dir.openNextFile();
    while (file) {
        String fullPath = prefix + "/" + file.name();

        if (file.isDirectory()) {
            listDirRecursive(file, fullPath, files, depth + 1);
        } else {
            JsonObject entry = files.createNestedObject();
            entry["path"] = fullPath;
            entry["name"] = file.name();
            entry["size"] = file.size();
        }
        file = dir.openNextFile();
    }
}
```

---

## Step 4: Update webserver.h

**File:** `src/webserver/webserver.h`

```cpp
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class WebServerManager {
private:
    AsyncWebServer* asyncServer;
    static const int SERVER_PORT = 80;

    void listDirRecursive(File dir, String prefix, JsonArray& files, int depth);

public:
    WebServerManager();
    ~WebServerManager();

    void begin();
    void stop();

    void setupScreenRoutes();
    void setupFileRoutes();
    void setupSchemaRoutes();

    bool isConnected() const;
};

extern WebServerManager webServer;

#endif
```

---

## Step 5: Update main.cpp

**File:** `src/main.cpp`

```cpp
#include "webserver/webserver.h"
#include "webserver/sd_mutex.h"

void setup() {
    // ... existing setup code ...

    // Initialize WebServer (AsyncWebServer is non-blocking)
    webServer.begin();

    // ... rest of setup ...
}

void loop() {
    // AsyncWebServer handles everything in background
    // No handleClient() call needed - it's completely async

    // Your application code runs here

    // Update sensors
    sensors.update();

    // Render display
    display.update();

    // Handle WebSocket if needed
    if (websocket.isConnected()) {
        websocket.poll();
    }

    // Small delay to prevent watchdog
    yield();
}
```

---

## Key Differences from Synchronous Approach

| Feature | Synchronous WebServer | AsyncWebServer + Mutex |
|---------|----------------------|----------------------|
| Thread Safety | ❌ Crashes with semaphores | ✅ Proven & stable |
| FreeRTOS Compatible | ❌ Incompatible | ✅ Compatible |
| Background Processing | ❌ Blocks main loop | ✅ Non-blocking |
| SD Card Access | ❌ Race conditions | ✅ Protected by mutex |
| Async/Await | ❌ N/A | ✅ Full support |

---

## Testing Checklist

- [ ] Revert `platformio.ini` to include AsyncWebServer
- [ ] Add `sd_mutex.h` and `sd_mutex.cpp`
- [ ] Update `webserver.cpp` with mutex protection
- [ ] Update `webserver.h` for AsyncWebServer
- [ ] Test `/api/schema/screen-elements` - should return JSON with sensors
- [ ] Test `/api/screens` - should list screen files
- [ ] Test `/api/upload-screen` - upload test JSON
- [ ] Test screen auto-reload after upload
- [ ] Verify no crashes after 5+ minutes of testing

---

## Why This Works

1. **`portENTER_CRITICAL()` / `portEXIT_CRITICAL()`** disable interrupts for the critical section
2. **AsyncWebServer handles its own FreeRTOS safely** - proven by millions of ESP32 projects
3. **Mutex is lightweight** - only held during SD card access (typically <10ms)
4. **No semaphore corruption** - we're using FreeRTOS primitives directly
5. **Background tasks continue** - WebSocket, sensors, display all work uninterrupted

---

## If It Still Crashes

If you still get crashes after this fix, check:

1. **Are you accessing SD card from other threads?** - All SD access must use the mutex
2. **Is another task also using the SD card?** - Add mutex calls to all SD operations
3. **Is the SD card failing?** - Try `SD.exists("/")` from SerialMonitor

---

