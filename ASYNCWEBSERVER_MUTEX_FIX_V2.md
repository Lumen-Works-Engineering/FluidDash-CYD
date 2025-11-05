# AsyncWebServer + Mutex Protection - CORRECTED VERSION

**Date:** November 4, 2025  
**Status:** Emergency Fix - Back to AsyncWebServer with Proper Synchronization  
**Version:** 2.0 - CORRECTED /api/download handler

---

## CRITICAL FIX - /api/download Handler

**Claude Code caught a critical bug in the original guide!**

### ❌ WRONG (Original Version):
```cpp
AsyncWebServerResponse *response = request->beginResponse(
    200, contentType, 
    [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
        return file.readBytes((char*)buffer, maxLen);  // File accessed AFTER mutex unlocked!
    }
);
SD_MUTEX_UNLOCK();  // ❌ Mutex released but file still streaming!
```

**Problem:** The streaming callback accesses the file AFTER the mutex is released, causing race conditions.

### ✅ CORRECT (This Version):
```cpp
// Read entire file into memory BEFORE unlocking mutex
String content = file.readString();
file.close();

SD_MUTEX_UNLOCK();  // Now safe - file is closed

// Send buffered response
request->send(200, contentType, content);
```

**Solution:** Read file into memory while mutex is held, then release mutex before sending response.

---

## Step 1: Update platformio.ini

**File:** `platformio.ini`

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
 * CRITICAL: All SD card operations MUST be completed before releasing mutex
 * Example:
 *   SD_MUTEX_LOCK();
 *   File f = SD.open("/path");
 *   String content = f.readString();  // Read completely
 *   f.close();                         // Close file
 *   SD_MUTEX_UNLOCK();                 // NOW safe to unlock
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

## Step 3: webserver.cpp - CORRECTED Route Handlers

**File:** `src/webserver/webserver.cpp`

```cpp
#include "webserver.h"
#include "sd_mutex.h"
#include "SD.h"

// ============================================
// /api/screens - List screen files (SAFE)
// ============================================
void WebServerManager::setupScreenRoutes() {
    asyncServer->on("/api/screens", HTTP_GET, [this](AsyncWebServerRequest *request) {
        SD_MUTEX_LOCK();

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
                if (!first) response += ",";
                response += "{\"name\":\"" + String(f.name()) + 
                           "\",\"size\":" + String(f.size()) + "}";
                first = false;
            }
            f.close();
            f = dir.openNextFile();
        }

        response += "]}";
        dir.close();

        SD_MUTEX_UNLOCK();  // ✅ Safe - all files closed, response buffered

        request->send(200, "application/json", response);
    });

    // ============================================
    // /api/screens/:filename - Download screen (SAFE)
    // ============================================
    asyncServer->on("^\\/api\\/screens\\/(.+)$", HTTP_GET, 
        [this](AsyncWebServerRequest *request) {

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

        // ✅ CORRECT: Read entire file while mutex is held
        String content = file.readString();
        file.close();

        SD_MUTEX_UNLOCK();  // ✅ Safe - file completely read and closed

        request->send(200, "application/json", content);
    });

    // ============================================
    // /api/upload-screen - Save screen JSON (SAFE)
    // ============================================
    asyncServer->on("/api/upload-screen", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {
            // Final response handler
            request->send(200, "application/json", 
                "{\"status\":\"saved\",\"message\":\"Screen uploaded successfully\"}");
        },
        NULL,  // No file upload handler
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, 
               size_t index, size_t total) {
            // Body data handler
            static String jsonBuffer;
            static String filename;

            if (index == 0) {
                jsonBuffer = "";
                // Extract filename from request args if available
                if (request->hasArg("filename")) {
                    filename = request->arg("filename");
                } else {
                    filename = "screen_" + String(millis()) + ".json";
                }
            }

            // Accumulate data
            for (size_t i = 0; i < len; i++) {
                jsonBuffer += (char)data[i];
            }

            // When complete, write to SD card
            if (index + len == total) {
                SD_MUTEX_LOCK();

                String path = "/screens/" + filename;
                File file = SD.open(path, FILE_WRITE);

                if (file) {
                    file.print(jsonBuffer);
                    file.close();

                    SD_MUTEX_UNLOCK();  // ✅ Safe - file closed

                    // Trigger screen reload
                    screenManager.loadScreens();
                } else {
                    SD_MUTEX_UNLOCK();
                    request->send(500, "text/plain", "Failed to create file");
                }

                jsonBuffer = "";  // Clear buffer
            }
        }
    );

    // ============================================
    // /api/delete-screen - Delete screen file (SAFE)
    // ============================================
    asyncServer->on("/api/delete-screen", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!request->hasArg("filename")) {
            request->send(400, "text/plain", "Missing filename parameter");
            return;
        }

        String filename = request->arg("filename");
        String path = "/screens/" + filename;

        SD_MUTEX_LOCK();

        if (!SD.exists(path)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "text/plain", "File not found");
            return;
        }

        bool success = SD.remove(path);

        SD_MUTEX_UNLOCK();  // ✅ Safe - operation complete

        if (success) {
            request->send(200, "application/json", "{\"status\":\"deleted\"}");
        } else {
            request->send(500, "text/plain", "Failed to delete file");
        }
    });
}

// ============================================
// /api/schema/screen-elements - Element discovery (NO SD ACCESS)
// ============================================
void WebServerManager::setupSchemaRoutes() {
    asyncServer->on("/api/schema/screen-elements", HTTP_GET, 
        [this](AsyncWebServerRequest *request) {

        // ✅ NO MUTEX NEEDED - no SD card access

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

        // Temperature sensors (auto-discover connected sensors)
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
// /api/files - List all files (SAFE)
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

        SD_MUTEX_UNLOCK();  // ✅ Safe - all operations complete

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // ============================================
    // /api/download - Download file (CORRECTED)
    // ============================================
    asyncServer->on("/api/download", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!request->hasArg("path")) {
            request->send(400, "text/plain", "Missing path parameter");
            return;
        }

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

        // Check file size limit (protect against huge files)
        size_t fileSize = file.size();
        if (fileSize > 100000) {  // 100KB limit
            file.close();
            SD_MUTEX_UNLOCK();
            request->send(413, "text/plain", "File too large (max 100KB)");
            return;
        }

        // ✅ CORRECT: Read entire file while mutex is held
        String content = file.readString();
        String filename = filepath.substring(filepath.lastIndexOf("/") + 1);
        file.close();

        SD_MUTEX_UNLOCK();  // ✅ Safe - file completely read and closed

        // Determine content type
        String contentType = "application/octet-stream";
        if (filename.endsWith(".json")) contentType = "application/json";
        else if (filename.endsWith(".txt")) contentType = "text/plain";
        else if (filename.endsWith(".html")) contentType = "text/html";

        // Send buffered response
        AsyncWebServerResponse *response = request->beginResponse(200, contentType, content);
        response->addHeader("Content-Disposition", "attachment; filename=" + filename);
        request->send(response);
    });

    // ============================================
    // /api/delete-file - Delete file (SAFE)
    // ============================================
    asyncServer->on("/api/delete-file", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!request->hasArg("path")) {
            request->send(400, "text/plain", "Missing path parameter");
            return;
        }

        String filepath = request->arg("path");

        SD_MUTEX_LOCK();

        if (!SD.exists(filepath)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "text/plain", "File not found");
            return;
        }

        bool success = SD.remove(filepath);

        SD_MUTEX_UNLOCK();  // ✅ Safe - operation complete

        if (success) {
            request->send(200, "text/plain", "File deleted successfully");
        } else {
            request->send(500, "text/plain", "Failed to delete file");
        }
    });
}

// ============================================
// Helper: Recursive directory listing (SAFE)
// ============================================
void WebServerManager::listDirRecursive(File dir, String prefix, 
                                         JsonArray& files, int depth) {
    if (depth > 3) return;  // Limit recursion

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

        file.close();  // ✅ Close immediately
        file = dir.openNextFile();
    }
}
```

---

## Step 4: webserver.h

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

## Step 5: main.cpp

**File:** `src/main.cpp`

```cpp
#include "webserver/webserver.h"
#include "webserver/sd_mutex.h"

void setup() {
    // ... existing setup code ...

    // Initialize SD card BEFORE webserver
    if (!SD.begin()) {
        Serial.println("SD Card init failed!");
    }

    // Initialize WebServer (AsyncWebServer is non-blocking)
    webServer.begin();

    Serial.println("WebServer started");
}

void loop() {
    // AsyncWebServer handles everything in background
    // NO handleClient() call needed!

    // Update sensors
    sensors.update();

    // Render display
    display.update();

    // Handle WebSocket
    if (websocket.isConnected()) {
        websocket.poll();
    }

    yield();  // Prevent watchdog
}
```

---

## Key Mutex Rules

### ✅ DO:
1. **Lock → Read/Write → Close → Unlock**
2. Buffer entire file contents before unlocking
3. Keep mutex held for < 50ms
4. Close all files before unlocking

### ❌ DON'T:
1. Never access files after unlocking mutex
2. Never use streaming callbacks with mutex
3. Never hold mutex for > 100ms
4. Never nest mutex locks

---

## File Size Limitations

Since we're buffering entire files in memory:

| File Type | Max Size | Reason |
|-----------|----------|--------|
| JSON screens | 10KB | Screen layouts are small |
| HTML/CSS | 50KB | Web interface files |
| Generic download | 100KB | Safety limit for RAM |

If you need larger files, implement chunked reading with mutex per chunk (advanced).

---

## Testing Checklist

- [ ] Update `platformio.ini`
- [ ] Add `sd_mutex.h` and `sd_mutex.cpp`
- [ ] Update `webserver.cpp` with corrected handlers
- [ ] Update `webserver.h`
- [ ] Update `main.cpp`
- [ ] Test `/api/schema/screen-elements`
- [ ] Test `/api/screens` (list files)
- [ ] Test `/api/screens/test.json` (download)
- [ ] Test `/api/upload-screen` (upload JSON)
- [ ] Test `/api/delete-screen`
- [ ] Test `/api/download?path=/screens/test.json`
- [ ] Verify no crashes after 10+ minutes

---

## Why This Version is Correct

1. **No streaming while mutex unlocked** - All file operations complete before unlock
2. **Files closed properly** - No dangling file handles
3. **Buffered responses** - Safe to send after mutex release
4. **Size limits** - Protect against OOM crashes
5. **Error handling** - Proper cleanup on failures

---

## What Claude Code Needs to Know

**Critical Pattern:**
```cpp
SD_MUTEX_LOCK();
File f = SD.open("/path");
String data = f.readString();  // Complete read
f.close();                      // Close file
SD_MUTEX_UNLOCK();              // NOW unlock

// Use 'data' safely here
request->send(200, "application/json", data);
```

**Never Do This:**
```cpp
SD_MUTEX_LOCK();
File f = SD.open("/path");
SD_MUTEX_UNLOCK();  // ❌ WRONG!

// File 'f' still open - RACE CONDITION!
request->send(200, "application/json", f.readString());
```

---
