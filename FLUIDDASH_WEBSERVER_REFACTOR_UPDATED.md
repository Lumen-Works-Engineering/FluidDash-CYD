# FluidDash CYD - WebServer Refactor & Screen Editor Implementation

**Date:** November 4, 2025  
**Objective:** Migrate from AsyncWebServer to synchronous WebServer library with integrated screen editor hosted on SD card  
**Status:** Planning Phase - Ready for Claude Code Implementation

---

## Executive Summary

This document provides Claude Code with a complete implementation plan to:

1. **Replace AsyncWebServer** with synchronous Arduino WebServer library (eliminates thread-safety issues with SD card access)
2. **Implement file management system** (upload, download, delete JSON screen files)
3. **Host screen editor on SD card** with real-time dynamic element discovery
4. **Add FTP server** for traditional file transfer workflows
5. **Maintain all existing functionality** while improving reliability and development workflow

**Key Benefit:** Zero SD card removal needed during development. Edit screens in browser, save to SD card, auto-reload display.

---

## Architecture Overview

### Current Issues (AsyncWebServer)

- Thread-safety conflicts between async callbacks (Core 0) and SD library (Core 1)
- File uploads crash the device
- Screen layout development workflow is cumbersome

### Solution (Synchronous WebServer)

- All HTTP handling runs in main loop context
- SD card operations are safe and predictable
- Proven by FluidNC implementation
- Less RAM overhead than async

### New Workflow

```
Editor.html (on SD) → Edit Screen JSON → /upload-screen → SD /screens/ → Auto-reload
```

### Updated Workflow Diagram

```
USER: Opens browser to http://fluidnc.local/editor.html
    ↓
EDITOR: Loads editor.html from SD card
    ↓
EDITOR: Calls /api/schema/screen-elements (discovers available sensors/variables)
    ↓
USER: Designs screen visually on 480×320 canvas
    ↓
USER: Clicks "Save to Device"
    ↓
EDITOR: POSTs JSON to /api/upload-screen
    ↓
WEBSERVER: Writes JSON to /screens/ folder on SD
    ↓
WEBSERVER: Calls screenManager.loadScreens() (auto-reload)
    ↓
DISPLAY: Updates with new screen layout instantly
    ↓
(Optional) USER: Exports JSON, commits to Git via FTP sync
```

---

## Phase 1: WebServer Library Migration

### Tasks

#### Task 1.1: Update platformio.ini

**File:** `platformio.ini`

**Changes:**

- Remove AsyncWebServer and AsyncTCP dependencies
- Keep WebSockets (already working)
- Add FTP library
- Adjust build flags for synchronous operation

```ini
[env:esp32dev]
platform = espressif32@^6.8.0
board = esp32dev
framework = arduino
board_build.flash_size = 4MB
board_build.filesystem = littlefs
monitor_speed = 115200
upload_port = COM9
monitor_port = COM9
lib_ldf_mode = deep+
lib_compat_mode = soft

build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT=0
    -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFiClientSecure/src
    -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/WiFi/src

lib_deps =
    adafruit/RTClib@^2.1.4
    lovyan03/LovyanGFX@^1.1.16
    links2004/WebSockets@2.7.1
    bblanchon/ArduinoJson@^7.2.0
    paulstoffregen/OneWire@^2.3.8
    milesburton/DallasTemperature@^3.11.0
    https://github.com/fa1ke5/ESP32_FTPServer_SD_MMC.git
```

**Rationale:**

- Remove: `https://github.com/me-no-dev/ESPAsyncWebServer.git` (causing thread issues)
- Remove: `https://github.com/me-no-dev/AsyncTCP.git` (not needed)
- Keep: WebSockets for FluidNC communication
- Add: FTP server for file transfer
- Add: `board_build.filesystem = littlefs` for better SD card compatibility

#### Task 1.2: Refactor webserver.h

**File:** `src/webserver/webserver.h`

```cpp
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <WebServer.h>
#include <ArduinoJson.h>

class WebServerManager {
private:
    WebServer* server;
    static const int SERVER_PORT = 80;

public:
    WebServerManager();
    ~WebServerManager();

    void begin();
    void handleClient();
    void stop();

    // Screen management endpoints
    void setupScreenRoutes();
    void setupFileRoutes();
    void setupSchemaRoutes();

    // Utility methods
    bool isConnected() const;
};

extern WebServerManager webServer;

#endif
```

**Key Changes:**

- Replace `AsyncWebServer` with `WebServer`
- Change from async callback to synchronous handler pattern
- Add route setup methods for organization

#### Task 1.3: Refactor webserver.cpp - Main Server Setup

**File:** `src/webserver/webserver.cpp`

```cpp
#include "webserver.h"
#include "../config/config.h"
#include "../utils/utils.h"
#include "SD.h"

WebServerManager webServer;

WebServerManager::WebServerManager() : server(nullptr) {}

WebServerManager::~WebServerManager() {
    if (server) {
        server->close();
        delete server;
    }
}

void WebServerManager::begin() {
    if (!server) {
        server = new WebServer(SERVER_PORT);
    }

    // Setup all routes
    setupScreenRoutes();
    setupFileRoutes();
    setupSchemaRoutes();

    // Serve static files from SD card
    server->serveStatic("/editor.html", SD, "/editor.html");
    server->serveStatic("/file-manager.html", SD, "/file-manager.html");
    server->serveStatic("/css", SD, "/css");
    server->serveStatic("/js", SD, "/js");

    // Root redirect to editor
    server->on("/", HTTP_GET, [this]() {
        server->send(200, "text/html", "<meta http-equiv='refresh' content='0;url=/editor.html'>");
    });

    // 404 handler
    server->onNotFound([this]() {
        server->send(404, "text/plain", "Not Found");
    });

    server->begin();
    log_i("WebServer started on port %d", SERVER_PORT);
}

void WebServerManager::handleClient() {
    if (server) {
        server->handleClient();
    }
}

void WebServerManager::stop() {
    if (server) {
        server->stop();
    }
}

bool WebServerManager::isConnected() const {
    return server != nullptr;
}
```

**Key Points:**

- Single `WebServer` instance (synchronous)
- `handleClient()` called from main loop (no background threads)
- `serveStatic()` serves files directly from SD card
- Root redirects to `/editor.html` (the new screen editor)
- All operations are blocking and safe

#### Task 1.4: Screen Routes Implementation

**File:** `src/webserver/webserver.cpp` - Add to class

```cpp
void WebServerManager::setupScreenRoutes() {
    // Get all screen files
    server->on("/api/screens", HTTP_GET, [this]() {
        DynamicJsonDocument doc(4096);
        JsonArray files = doc.createNestedArray("files");

        File dir = SD.open("/screens");
        if (!dir) {
            server->send(404, "application/json", "{}");
            return;
        }

        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory() && String(f.name()).endsWith(".json")) {
                JsonObject file = files.createNestedObject();
                file["name"] = f.name();
                file["size"] = f.size();
                file["modified"] = (long)f.getLastWrite();
            }
            f = dir.openNextFile();
        }

        dir.close();

        String response;
        serializeJson(doc, response);
        server->send(200, "application/json", response);
    });

    // Download specific screen file
    server->on("/api/screens/:filename", HTTP_GET, [this]() {
        String filename = server->pathArg(0);
        String path = "/screens/" + filename;

        if (!SD.exists(path)) {
            server->send(404, "text/plain", "File not found");
            return;
        }

        File file = SD.open(path);
        if (!file) {
            server->send(500, "text/plain", "Cannot open file");
            return;
        }

        server->sendHeader("Content-Type", "application/json");
        server->sendHeader("Content-Disposition", 
            "inline; filename=" + filename);

        // Stream file to client
        while (file.available()) {
            server->write(file.read());
        }
        file.close();
    });

    // Upload/Save screen file - FROM EDITOR
    server->on("/api/upload-screen", HTTP_POST, [this]() {
        String body = server->arg("plain");

        // Parse JSON body
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            server->send(400, "application/json", 
                "{"error": "Invalid JSON"}");
            return;
        }

        // Extract filename from data
        String filename = doc["filename"] | "new_screen.json";
        JsonObject screenData = doc["data"].as<JsonObject>();

        String path = "/screens/" + filename;

        // Validate it's valid screen JSON
        if (!screenData["elements"].is<JsonArray>()) {
            server->send(400, "application/json", 
                "{"error": "Invalid screen format"}");
            return;
        }

        // Write file
        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            server->send(500, "application/json", 
                "{"error": "Cannot create file"}");
            return;
        }

        // Serialize just the screen data (not the filename wrapper)
        serializeJson(screenData, file);
        file.close();

        // Reload screens
        screenManager.loadScreens();

        server->send(200, "application/json", 
            "{"status": "saved", "file": "" + filename + ""}");
    });

    // Delete screen file
    server->on("/api/delete-screen", HTTP_POST, [this]() {
        String filename = server->arg("filename");
        String path = "/screens/" + filename;

        if (!SD.exists(path)) {
            server->send(404, "text/plain", "File not found");
            return;
        }

        SD.remove(path);
        server->send(200, "text/plain", "File deleted");
    });

    // Reload screens endpoint
    server->on("/api/reload-screens", HTTP_GET, [this]() {
        screenManager.loadScreens();
        server->send(200, "text/plain", "Screens reloaded");
    });
}
```

#### Task 1.5: Schema/Element Discovery Routes

**File:** `src/webserver/webserver.cpp` - Add to class

```cpp
void WebServerManager::setupSchemaRoutes() {
    // Return available screen elements schema for editor
    server->on("/api/schema/screen-elements", HTTP_GET, [this]() {
        DynamicJsonDocument doc(8192);

        // Coordinate data sources
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

        // Status fields
        JsonArray status = doc.createNestedArray("status");
        status.add("machineState");
        status.add("feedRate");
        status.add("spindleRPM");

        // System fields
        JsonArray system = doc.createNestedArray("system");
        system.add("psuVoltage");
        system.add("fanSpeed");
        system.add("ipAddress");
        system.add("ssid");
        system.add("deviceName");
        system.add("fluidncIP");

        // Element types available
        JsonArray types = doc.createNestedArray("elementTypes");
        types.add("rect");
        types.add("line");
        types.add("text");
        types.add("dynamic");
        types.add("temp");
        types.add("status");
        types.add("progress");
        types.add("graph");

        // Colors reference (RGB565 hex)
        JsonObject colors = doc.createNestedObject("colors");
        colors["black"] = "0000";
        colors["white"] = "FFFF";
        colors["red"] = "F800";
        colors["green"] = "07E0";
        colors["blue"] = "001F";
        colors["yellow"] = "FFE0";
        colors["cyan"] = "07FF";
        colors["magenta"] = "F81F";
        colors["darkgray"] = "4A49";

        String response;
        serializeJson(doc, response);
        server->send(200, "application/json", response);
    });
}
```

#### Task 1.6: File Management Routes

**File:** `src/webserver/webserver.cpp` - Add to class

```cpp
void WebServerManager::setupFileRoutes() {
    // List all files on SD card
    server->on("/api/files", HTTP_GET, [this]() {
        DynamicJsonDocument doc(8192);
        JsonArray files = doc.createNestedArray("files");

        File root = SD.open("/");
        if (!root) {
            server->send(500, "text/plain", "Cannot open SD root");
            return;
        }

        listDirRecursive(root, "", files, 0);
        root.close();

        String response;
        serializeJson(doc, response);
        server->send(200, "application/json", response);
    });

    // Download file (generic)
    server->on("/api/download", HTTP_GET, [this]() {
        String filepath = server->arg("path");

        if (!SD.exists(filepath)) {
            server->send(404, "text/plain", "File not found");
            return;
        }

        File file = SD.open(filepath);
        if (!file) {
            server->send(500, "text/plain", "Cannot open file");
            return;
        }

        String filename = filepath.substring(filepath.lastIndexOf("/") + 1);
        server->sendHeader("Content-Disposition", 
            "attachment; filename=" + filename);
        server->sendHeader("Content-Length", String(file.size()));

        // Determine content type
        String contentType = "application/octet-stream";
        if (filename.endsWith(".json")) contentType = "application/json";
        else if (filename.endsWith(".txt")) contentType = "text/plain";
        else if (filename.endsWith(".html")) contentType = "text/html";

        server->sendHeader("Content-Type", contentType);

        // Stream file
        int sent = 0;
        uint8_t buffer[1024];
        while (file.available() && sent < file.size()) {
            int toRead = min(1024, (int)file.size() - sent);
            int read = file.readBytes((char*)buffer, toRead);
            server->write(buffer, read);
            sent += read;
        }

        file.close();
    });

    // Delete file
    server->on("/api/delete-file", HTTP_POST, [this]() {
        String filepath = server->arg("path");

        if (!SD.exists(filepath)) {
            server->send(404, "text/plain", "File not found");
            return;
        }

        if (SD.remove(filepath)) {
            server->send(200, "text/plain", "File deleted");
        } else {
            server->send(500, "text/plain", "Cannot delete file");
        }
    });

    // Get SD card usage
    server->on("/api/disk-usage", HTTP_GET, [this]() {
        DynamicJsonDocument doc(256);

        // Note: ESP32 SD card usage is not easily accessible via SD lib
        // This is a placeholder - implement based on your SD library
        doc["total"] = 1900000000;  // Typical SD card
        doc["used"] = 500000000;
        doc["free"] = 1400000000;

        String response;
        serializeJson(doc, response);
        server->send(200, "application/json", response);
    });
}

// Helper function for recursive directory listing
void WebServerManager::listDirRecursive(File dir, String prefix, 
                                         JsonArray& files, int depth) {
    if (depth > 3) return;  // Limit recursion depth

    File file = dir.openNextFile();
    while (file) {
        String fullPath = prefix + "/" + file.name();

        if (file.isDirectory()) {
            // Recursively list subdirectories
            listDirRecursive(file, fullPath, files, depth + 1);
        } else {
            JsonObject entry = files.createNestedObject();
            entry["path"] = fullPath;
            entry["name"] = file.name();
            entry["size"] = file.size();
            entry["modified"] = (long)file.getLastWrite();
        }

        file = dir.openNextFile();
    }
}
```

---

## Phase 2: Screen Editor Implementation

### Task 2.1: Deploy editor.html to SD Card

**File:** `editor.html` (provided separately - place at `/editor.html` on SD card)

**Features Included:**

- **Dynamic Schema Discovery**: Loads `/api/schema/screen-elements` on startup
- **Visual Canvas Designer**: 480×320 display with click-and-drag element creation
- **Real-time Properties Panel**: Edit position, size, color, data source
- **Automatic Dropdowns**: Data sources populate from available sensors/variables
- **Save to Device**: POSTs JSON to `/api/upload-screen`
- **Load from Device**: Browse and load existing screens
- **Export JSON**: Download to PC for version control
- **New Screen**: Start fresh
- **Modern UI**: Gradient header, responsive layout, status messages

**File Structure:**

```
/editor.html                   Main screen editor
/api/schema/screen-elements    Dynamic element discovery endpoint
/api/upload-screen             Save endpoint (called by editor)
/api/screens                   List endpoint (called by editor)
/api/screens/:filename         Load endpoint (called by editor)
```

**Usage:**

1. Copy `editor.html` to SD card at root: `/editor.html`
2. Access via browser: `http://fluidnc.local/editor.html`
3. Design screen visually
4. Click "Save to Device" → automatic reload
5. Screens stored at `/screens/*.json`

**Key Editor Features:**

- Loads available data sources from device API (no hardcoding!)
- Shows connected DS18B20 sensors
- Displays coordinate, status, and system variables
- Real-time canvas preview with selection highlighting
- Element list with quick selection
- Properties panel for fine-tuning
- Status messages for user feedback

---

## Phase 3: FTP Server Integration

### Task 3.1: FTP Server in main.cpp

**File:** `src/main.cpp`

```cpp
#include <ESP32FTP.h>

ESP32FTP ftpServer;
unsigned long lastFTPCheck = 0;

void setup() {
    // ... existing setup code ...

    // Start FTP server
    ftpServer.begin("fluiddash", "password123");  // username, password
    log_i("FTP Server started");
}

void loop() {
    // Handle WebServer (synchronous, safe)
    webServer.handleClient();

    // Handle FTP (synchronous, runs in main loop)
    if (millis() - lastFTPCheck > 1000) {  // Check every second
        ftpServer.handleFTP();
        lastFTPCheck = millis();
    }

    // ... rest of main loop ...

    // Display refresh, sensor updates, etc.
}
```

**Usage:**

- Windows File Explorer: `ftp://192.168.x.x` → Login with fluiddash/password123
- FileZilla: New site → Protocol: FTP → Host: 192.168.x.x → Username: fluiddash → Password: password123
- Command line: `ftp 192.168.x.x` → `fluiddash` → `password123`

---

## Phase 4: Main Loop Integration

### Task 4.1: Update main.cpp

**File:** `src/main.cpp`

```cpp
void setup() {
    // ... existing hardware initialization ...

    // Initialize WebServer with new synchronous library
    webServer.begin();

    // Initialize other systems...
}

void loop() {
    // === PRIORITY 1: Handle WebServer (Synchronous) ===
    webServer.handleClient();  // Non-blocking for simple requests

    // === PRIORITY 2: Handle FTP (Synchronous) ===
    // ftpServer.handleFTP();  // Optional, only if needed

    // === PRIORITY 3: Main Application Loop ===

    // Update sensors
    sensors.update();

    // Handle screen rendering
    display.update();

    // Handle FluidNC WebSocket connection
    if (websocket.isConnected()) {
        websocket.poll();
    }

    // Yield to prevent watchdog triggers
    yield();
}
```

**Key Point:** `handleClient()` is non-blocking for simple GET/POST requests, so it doesn't interrupt your main application flow.

---

## Complete File Operations Workflow

### From Editor

```
User opens http://fluidnc.local/editor.html
    ↓ Loads /editor.html from SD card
    ↓ Calls /api/schema/screen-elements (discovers sensors)
    ↓ User designs screen visually
    ↓ Clicks "Save to Device"
    ↓ Calls /api/upload-screen with JSON
    ↓ Webserver writes /screens/screenname.json
    ↓ Calls screenManager.loadScreens()
    ↓ Display updates automatically
```

### From File Manager (Future)

```
User opens http://fluidnc.local/file-manager.html
    ↓ Lists all files via /api/files
    ↓ Can download via /api/download?path=/screens/monitor.json
    ↓ Can delete via /api/delete-file
    ↓ Can upload new files
```

### From FTP (For Git Sync)

```
User opens Windows File Explorer
    ↓ Navigates to ftp://192.168.x.x
    ↓ Logs in with fluiddash/password123
    ↓ Browses /screens folder
    ↓ Downloads JSON files to PC
    ↓ Commits to Git
    ↓ Pushes to GitHub
```

---

## Implementation Checklist

### Phase 1: WebServer Migration

- [ ] Update `platformio.ini` - remove AsyncWebServer, add FTP library
- [ ] Create new `webserver.h` with synchronous WebServer
- [ ] Rewrite `webserver.cpp` with route handlers
- [ ] Implement screen routes (`/api/screens/*`)
- [ ] Implement schema routes (`/api/schema/*`)
- [ ] Implement file routes (`/api/files*`)
- [ ] Update `main.cpp` to call `handleClient()` in loop
- [ ] Test basic web server functionality
- [ ] Test SD card file operations

### Phase 2: Screen Editor Deployment

- [ ] Copy `editor.html` to `/editor.html` on SD card
- [ ] Test editor loads at http://fluidnc.local/editor.html
- [ ] Verify schema discovery works (data sources populate)
- [ ] Test element creation on canvas
- [ ] Test save to device
- [ ] Verify file saved to `/screens/`
- [ ] Test auto-reload on display
- [ ] Test load existing screen
- [ ] Test export JSON

### Phase 3: File Manager

- [ ] Create `file-manager.html`
- [ ] Implement file listing
- [ ] Implement file download
- [ ] Implement file delete

### Phase 4: FTP Integration

- [ ] Add FTP library to platformio.ini
- [ ] Initialize FTP in setup()
- [ ] Call FTP handler in loop
- [ ] Test FTP file transfer from Windows Explorer

### Phase 5: Testing

- [ ] Web editor loads and displays dynamic elements
- [ ] Save screen JSON via editor
- [ ] Load screen JSON in editor
- [ ] Download files via web manager
- [ ] Delete files via web manager
- [ ] FTP file transfers work
- [ ] No SD card crashes
- [ ] Screen reloads automatically after save

---

## Critical Code Sections

### webserver.h Header

```cpp
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <WebServer.h>
#include <ArduinoJson.h>
#include "SD.h"

class WebServerManager {
private:
    WebServer* server;
    static const int SERVER_PORT = 80;
    void listDirRecursive(File dir, String prefix, JsonArray& files, int depth);

public:
    WebServerManager();
    ~WebServerManager();
    void begin();
    void handleClient();
    void stop();
    void setupScreenRoutes();
    void setupFileRoutes();
    void setupSchemaRoutes();
    bool isConnected() const;
};

extern WebServerManager webServer;

#endif
```

### Platform Update

Change from:

```ini
https://github.com/me-no-dev/ESPAsyncWebServer.git
https://github.com/me-no-dev/AsyncTCP.git
```

To:

```ini
https://github.com/fa1ke5/ESP32_FTPServer_SD_MMC.git
```

---

## Testing Strategy

### Local Testing (No Device)

1. Verify code compiles without errors
2. Check for syntax issues
3. Review API endpoints

### Device Testing (First Boot)

1. Flash firmware
2. Connect to device via `http://fluiddash.local`
3. Verify redirect to `/editor.html` works
4. Check `/api/schema/screen-elements` returns data
5. Create test screen JSON on canvas
6. Upload via editor
7. Verify file saved to `/screens/test.json`
8. Reload and verify screen appears
9. Test FTP file transfer
10. Test export/import workflow

### Workflow Testing

1. Edit screen in browser
2. Save to device
3. Observe instant reload
4. Verify display updates with new layout
5. Download JSON file
6. Commit to Git

---

## Screen Editor Features Summary

### Canvas & Drawing

- 480×320 pixel display preview
- Draw rectangles, lines, text, and dynamic elements
- Click-and-drag interface
- Real-time visual feedback

### Element Properties

- Position (X, Y)
- Size (Width, Height)
- Color (RGB565 hex)
- Line width
- Data source selection (populated from device)
- Static text or dynamic value

### Data Source Discovery

- Coordinates: wposX, wposY, wposZ, wposA, posX, posY, posZ, posA
- Temperatures: temp0-temp3 (auto-discovered from connected sensors)
- Status: machineState, feedRate, spindleRPM
- System: psuVoltage, fanSpeed, ipAddress, ssid, deviceName, fluidncIP

### File Operations

- **New**: Create blank screen
- **Load**: Browse and load existing screens from device
- **Save**: Export directly to device SD card
- **Export**: Download JSON to PC for Git version control

### User Experience

- Modern gradient UI
- Status messages (success/error/info)
- Responsive design
- Element list with quick selection
- Properties panel for fine-tuning

---

## Known Limitations & Notes

### Synchronous WebServer

- Blocks main loop during HTTP request handling
- For small requests (<1-2KB), impact is negligible
- Not suitable for high-frequency data streaming (but not your use case)
- Recommended for embedded systems with single connected client

### SD Card File System

- LittleFS recommended over SPIFFS for reliability
- Max file size depends on available RAM for JSON parsing (8-16KB typical)
- Consider flash wear for frequent file updates (not an issue for screen layouts)

### FTP Server

- Single user at a time
- No authentication beyond username/password
- Uses Core 1 when called, no threading conflicts

### Editor HTML

- Loads entirely from SD card or PC
- No external CDN dependencies required
- Self-contained single HTML file
- Works offline once loaded

---

## References

- ESP32 WebServer Documentation: https://github.com/espressif/arduino-esp32
- FluidNC File Management: https://github.com/bdring/FluidNC
- ESP32 FTP Server: https://github.com/fa1ke5/ESP32_FTPServer_SD_MMC
- Current Editor: `editor.html` (included)

---

## Approval & Next Steps

**Reviewed by:** John Sparks  
**Approved for Claude Code:** YES

**Immediate Next Steps:**

1. Create new webserver.h with synchronous WebServer class
2. Implement webserver.cpp with route handlers
3. Update platformio.ini
4. Copy editor.html to SD card
5. Test compilation and basic functionality

---

**Questions for Claude Code:**

- Do you need any modifications to the editor.html functionality?
- Should we prioritize FTP or File Manager implementation first?
- Any issues with the synchronous WebServer approach?

---

## Editor HTML Details

**File:** `editor.html` (33KB single file)

### Key Features

1. **Dynamic Schema Discovery**
   
   - Calls `/api/schema/screen-elements` on load
   - Populates all dropdowns from device
   - Discovers connected sensors automatically
   - No hardcoding of element names

2. **Visual Canvas Designer**
   
   - 480×320 display preview
   - Element creation by type
   - Click-and-drag drawing
   - Real-time visual feedback
   - Selection highlighting with handles

3. **Properties Panel**
   
   - Real-time property editing
   - Grouped data source selection
   - Color picker (RGB565)
   - Size and position controls

4. **File Operations**
   
   - Save directly to device (`/api/upload-screen`)
   - Load from device (`/api/screens`)
   - Export JSON to PC
   - Browse existing screens

5. **Modern UI**
   
   - Gradient purple header
   - Responsive sidebar layout
   - Status message feedback
   - Element list with selection
   - Professional styling

### API Endpoints Called

```
GET  /api/schema/screen-elements    (on load, discover available elements)
GET  /api/screens                   (list screens)
GET  /api/screens/:filename         (load screen)
POST /api/upload-screen             (save screen)
```

### File Placement

```
SD Card /editor.html
```

### Access

```
Browser: http://fluidnc.local/editor.html
```

### Workflow

```
Open Editor → Loads Schema → Design → Save → Auto-reload Display
```

---
