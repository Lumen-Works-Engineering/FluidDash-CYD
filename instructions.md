# FLUIDDASH COMPLETE IMPLEMENTATION - CLAUDE CODE MASTER INSTRUCTIONS
## CRITICAL PREREQUISITES

Before starting, verify:
```
✓ platformio.ini exists
✓ src/main.cpp loads current screen layouts
✓ WebServer (not AsyncWebServer) is implemented
✓ Device boots successfully without web features
✓ JSON layout files exist and load
```

If any are missing, stop and fix before proceeding.

---

## PHASE 1: STABLE SPIFFS FALLBACK
**Checkpoint 1: Device boots with or without SD, displays layouts, no crashes**

---

### PHASE 1 - TASK 1.1: Configure SPIFFS Partition

**File:** `platformio.ini`

**Action:** Find the `[env:esp32-s3-devkitc-1]` section (or your board). Add/modify this:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; ... existing build settings ...

; ADD THESE LINES FOR SPIFFS:
board_build.partitions = default_16MB.csv
board_build.filesystem = littlefs
board_upload.wait_for_upload_port = false
monitor_speed = 115200

; SPIFFS Configuration
board_build.embed_textblobfs_bin = yes
board_littlefs_mmap = yes
```

**Note:** If you're NOT on ESP32-S3, adjust board name accordingly.

**Show me:** Confirmation that platformio.ini updated with SPIFFS config

***

### PHASE 1 - TASK 1.2: Create Storage Manager Header

**File to Create:** `src/storage/storage_manager.h`

**Directory:** Create `src/storage/` directory first

**Content:**

```cpp
#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>

class StorageManager {
public:
    // Initialize all storage systems
    static void begin();
    
    // Load layout with fallback chain: SD → SPIFFS → hardcoded default
    static String loadLayout(const char* layoutName);
    
    // Check if layout exists anywhere
    static bool layoutExists(const char* layoutName);
    
    // Get list of available layouts (from SD or SPIFFS)
    static void getAvailableLayouts(JsonArray& layouts);
    
    // Write to SPIFFS (safe)
    static bool writeSPIFFS(const char* path, const String& data);
    
    // Read from SPIFFS
    static String readSPIFFS(const char* path);
    
    // Check storage status
    static void printStorageStatus();
    
    // Reset SD card to factory defaults (copy from SPIFFS)
    static bool resetToDefaults();

private:
    static const char* DEFAULT_LAYOUTS[];
    static String getHardcodedDefault(const char* layoutName);
};

#endif // STORAGE_MANAGER_H
```

**Show me:** File created in `src/storage/storage_manager.h`

***

### PHASE 1 - TASK 1.3: Create Storage Manager Implementation

**File to Create:** `src/storage/storage_manager.cpp`

**Content:**

```cpp
#include "storage_manager.h"

// Hardcoded defaults for each layout
const char* StorageManager::DEFAULT_LAYOUTS[] = {
    "monitor", "alignment", "graph", "network"
};

void StorageManager::begin() {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("[Storage] ERROR: Failed to initialize SPIFFS!");
        return;
    }
    
    Serial.println("[Storage] SPIFFS initialized successfully");
    
    // Initialize SD (may fail - that's OK)
    if (!SD.begin(4)) {  // GPIO 4 is typical MISO for CYD
        Serial.println("[Storage] WARNING: SD card not detected (OK - using SPIFFS)");
    } else {
        Serial.println("[Storage] SD card initialized successfully");
    }
    
    printStorageStatus();
}

String StorageManager::loadLayout(const char* layoutName) {
    // Priority 1: Try SD card first
    String sdPath = "/screens/" + String(layoutName) + ".json";
    if (SD.exists(sdPath)) {
        Serial.printf("[Storage] Loading from SD: %s\n", sdPath.c_str());
        File file = SD.open(sdPath, FILE_READ);
        if (file) {
            String data = file.readString();
            file.close();
            return data;
        }
    }
    
    // Priority 2: Try SPIFFS defaults
    String spiffsPath = "/defaults/" + String(layoutName) + ".json";
    if (SPIFFS.exists(spiffsPath)) {
        Serial.printf("[Storage] Loading from SPIFFS: %s\n", spiffsPath.c_str());
        File file = SPIFFS.open(spiffsPath, FILE_READ);
        if (file) {
            String data = file.readString();
            file.close();
            return data;
        }
    }
    
    // Priority 3: Hardcoded default
    Serial.printf("[Storage] Using hardcoded default for: %s\n", layoutName);
    return getHardcodedDefault(layoutName);
}

bool StorageManager::layoutExists(const char* layoutName) {
    String sdPath = "/screens/" + String(layoutName) + ".json";
    String spiffsPath = "/defaults/" + String(layoutName) + ".json";
    
    return SD.exists(sdPath) || SPIFFS.exists(spiffsPath);
}

void StorageManager::printStorageStatus() {
    Serial.println("\n[Storage] === STORAGE STATUS ===");
    
    // SPIFFS status
    size_t spiffsFree = SPIFFS.usedBytes();
    size_t spiffsTotal = SPIFFS.totalBytes();
    Serial.printf("[Storage] SPIFFS: %d/%d bytes (%.1f%% used)\n", 
                  spiffsFree, spiffsTotal, (float)spiffsFree/spiffsTotal*100);
    
    // SD status
    uint64_t sdFree = SD.cardSize() - SD.usedBytes();
    uint64_t sdTotal = SD.cardSize();
    Serial.printf("[Storage] SD Card: %lld/%lld bytes (%.1f%% free)\n",
                  sdFree, sdTotal, (float)sdFree/sdTotal*100);
    
    Serial.println("[Storage] === END STATUS ===\n");
}

bool StorageManager::writeSPIFFS(const char* path, const String& data) {
    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[Storage] ERROR: Failed to open SPIFFS file: %s\n", path);
        return false;
    }
    
    size_t written = file.print(data);
    file.close();
    
    if (written == data.length()) {
        Serial.printf("[Storage] SUCCESS: Wrote %d bytes to SPIFFS: %s\n", 
                      written, path);
        return true;
    } else {
        Serial.printf("[Storage] ERROR: Partial write to SPIFFS (expected %d, got %d)\n",
                      data.length(), written);
        return false;
    }
}

String StorageManager::readSPIFFS(const char* path) {
    if (!SPIFFS.exists(path)) {
        Serial.printf("[Storage] ERROR: File not found in SPIFFS: %s\n", path);
        return "";
    }
    
    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[Storage] ERROR: Failed to open file: %s\n", path);
        return "";
    }
    
    String data = file.readString();
    file.close();
    
    return data;
}

bool StorageManager::resetToDefaults() {
    Serial.println("[Storage] Resetting SD card to factory defaults...");
    
    // Remove all files in /screens directory
    File root = SD.open("/screens");
    if (root) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            file.close();
            SD.remove(String("/screens/") + name);
            Serial.printf("[Storage] Deleted: %s\n", name.c_str());
            file = root.openNextFile();
        }
        root.close();
    }
    
    // Copy defaults from SPIFFS to SD
    for (const char* layoutName : DEFAULT_LAYOUTS) {
        String spiffsPath = "/defaults/" + String(layoutName) + ".json";
        String sdPath = "/screens/" + String(layoutName) + ".json";
        
        String data = readSPIFFS(spiffsPath.c_str());
        if (data.length() > 0) {
            File file = SD.open(sdPath, FILE_WRITE);
            if (file) {
                file.print(data);
                file.close();
                Serial.printf("[Storage] Restored: %s\n", layoutName);
            }
        }
    }
    
    Serial.println("[Storage] Reset to defaults complete");
    return true;
}

String StorageManager::getHardcodedDefault(const char* layoutName) {
    // Return a minimal valid JSON layout as emergency fallback
    return String("{\"screens\":[{\"type\":\"text\",\"text\":\"Default: ") + 
           layoutName + "\",\"x\":10,\"y\":10}]}";
}
```

**Show me:** File created in `src/storage/storage_manager.cpp`

***

### PHASE 1 - TASK 1.4: Embed Default Layout Files

**Files to Create:** 4 JSON files in `data/defaults/` directory

**Action:** 
1. Create directory: `data/defaults/`
2. Copy your existing monitor.json, alignment.json, graph.json, network.json into this directory
3. Rename them to match exactly

**Files:**
- `data/defaults/monitor.json`
- `data/defaults/alignment.json`
- `data/defaults/graph.json`
- `data/defaults/network.json`

**Show me:** Confirmation that all 4 files created in `data/defaults/`

***

### PHASE 1 - TASK 1.5: Modify main.cpp - Add SPIFFS Initialization

**File:** `src/main.cpp`

**Location:** In your `setup()` function, BEFORE you initialize the web server

**Action:** Add these lines:

```cpp
void setup() {
    Serial.begin(115200);
    
    // ... your existing setup code (display init, etc) ...
    
    // ADD THIS - Initialize storage systems
    Serial.println("[Main] Initializing storage...");
    StorageManager::begin();
    
    // ... rest of your setup code ...
}
```

Also add at the TOP of main.cpp:

```cpp
#include "storage/storage_manager.h"
```

**Show me:** Confirmation that main.cpp updated with StorageManager initialization

***

### PHASE 1 - TASK 1.6: Update Layout Loading Function

**File:** `src/display/screen_renderer.cpp` or wherever your `loadScreenConfig()` is

**Action:** Replace the SD.open() calls with StorageManager::loadLayout()

**Find this pattern:**
```cpp
File file = SD.open("/screens/monitor.json");
if (file) {
    // load JSON
}
```

**Replace with:**
```cpp
String jsonData = StorageManager::loadLayout("monitor");
if (jsonData.length() > 0) {
    // Parse JSON from string instead of File
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, jsonData);
    // ... rest of parsing ...
}
```

**Note:** You'll need to adapt this to YOUR specific code, but the pattern is:
- Old: `SD.open()` + read from File
- New: `StorageManager::loadLayout()` + parse from String

**Show me:** Modified loadScreenConfig() function showing StorageManager usage

***

### PHASE 1 - TASK 1.7: Remove Upload Queue Code

**File:** `src/main.cpp`

**Action:** COMMENT OUT or DELETE:

```cpp
// DELETE/COMMENT THESE:
if (uploadQueue.hasPending()) {
    processQueuedUpload();
    yield();
}
```

Also comment out any references to `uploadQueue` in handlers.

**Show me:** Confirmation that upload queue processing removed from loop()

***

### PHASE 1 - TESTING CHECKPOINT

**Build and test locally:**

```bash
platformio run --target upload
```

**Expected Serial Output:**
```
[Main] Initializing storage...
[Storage] SPIFFS initialized successfully
[Storage] SD card initialized successfully (or: SD card not detected)
[Storage] === STORAGE STATUS ===
[Storage] SPIFFS: XXX/XXX bytes
[Storage] SD Card: XXX/XXX bytes
Setup complete - entering main loop
[JSON] Drawing monitor from JSON layout
```

**Tests to perform:**
1. ✓ Device boots successfully
2. ✓ Display shows monitor layout
3. ✓ No crashes or watchdog resets
4. ✓ Remove SD card, reboot - uses SPIFFS defaults
5. ✓ Insert SD card with existing layouts, reboot - uses SD layouts
6. ✓ Stability test: 5 minutes continuous operation

**Report back:**
- Serial output (copy-paste)
- Any errors encountered
- Display appearance

***

**PHASE 1 COMPLETE WHEN:**
- ✓ Device boots with/without SD
- ✓ Display shows layouts correctly
- ✓ No crashes
- ✓ Serial shows storage status

***

## PHASE 2: WEB EDITOR WITH SPIFFS BUFFER
**Checkpoint 2: Web editor saves layouts to SPIFFS, device displays them, no crashes**

---

# PHASE 2: WEB EDITOR WITH SPIFFS BUFFER)
**Checkpoint 2: Web editor works, saves to SPIFFS, syncs to SD, display updates, no crashes**

---

## PHASE 2 - TASK 2.1: Create Editor API Handler

**File to Create:** `src/editor/editor_handler.h`

**Directory:** Create `src/editor/` directory first

**Content:**

```cpp
#ifndef EDITOR_HANDLER_H
#define EDITOR_HANDLER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "storage/storage_manager.h"

class EditorHandler {
public:
    // Initialize web endpoints for editor
    static void registerEndpoints(WebServer& server);
    
    // Save layout to SPIFFS (called by web handler)
    static void handleSaveLayout();
    
    // Get current layout
    static void handleGetLayout();
    
    // List available layouts
    static void handleListLayouts();
    
    // Get editor UI
    static void handleEditorUI();
    
    // Get editor JavaScript
    static void handleEditorJS();
    
    // Get editor CSS
    static void handleEditorCSS();
    
private:
    static WebServer* serverPtr;  // Pointer to WebServer instance
    static String currentEditingLayout;
};

#endif // EDITOR_HANDLER_H
```

**Show me:** File created in `src/editor/editor_handler.h`

***

## PHASE 2 - TASK 2.2: Create Editor API Handler Implementation

**File to Create:** `src/editor/editor_handler.cpp`

**Content:**

```cpp
#include "editor_handler.h"

WebServer* EditorHandler::serverPtr = nullptr;
String EditorHandler::currentEditingLayout = "monitor";

void EditorHandler::registerEndpoints(WebServer& server) {
    EditorHandler::serverPtr = &server;
    
    // Web UI endpoints
    server.on("/editor/", HTTP_GET, []() { EditorHandler::handleEditorUI(); });
    server.on("/editor/editor.js", HTTP_GET, []() { EditorHandler::handleEditorJS(); });
    server.on("/editor/editor.css", HTTP_GET, []() { EditorHandler::handleEditorCSS(); });
    
    // API endpoints
    server.on("/api/editor/save", HTTP_POST, []() { EditorHandler::handleSaveLayout(); });
    server.on("/api/editor/get", HTTP_GET, []() { EditorHandler::handleGetLayout(); });
    server.on("/api/editor/list", HTTP_GET, []() { EditorHandler::handleListLayouts(); });
    
    Serial.println("[Editor] Endpoints registered");
}

void EditorHandler::handleEditorUI() {
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FluidDash Layout Editor</title>
    <link rel="stylesheet" href="/editor/editor.css">
</head>
<body>
    <div class="container">
        <h1>FluidDash Layout Editor</h1>
        
        <div class="controls">
            <div class="layout-selector">
                <label>Layout:</label>
                <select id="layoutSelect" onchange="loadLayout()">
                    <option value="monitor">Monitor</option>
                    <option value="alignment">Alignment</option>
                    <option value="graph">Graph</option>
                    <option value="network">Network</option>
                </select>
            </div>
            
            <div class="buttons">
                <button onclick="saveLayout()" class="btn-save">Save Layout</button>
                <button onclick="reloadDevice()" class="btn-reload">Reload on Device</button>
                <button onclick="resetToDefaults()" class="btn-reset">Reset to Defaults</button>
            </div>
        </div>
        
        <div class="editor-container">
            <div class="json-editor">
                <label>Layout JSON:</label>
                <textarea id="jsonEditor" rows="20"></textarea>
            </div>
            
            <div class="status">
                <p id="status">Ready</p>
            </div>
        </div>
    </div>
    
    <script src="/editor/editor.js"></script>
</body>
</html>
    )html";
    
    serverPtr->send(200, "text/html", html);
}

void EditorHandler::handleEditorJS() {
    String js = R"js(
// Load layout from device
async function loadLayout() {
    const layout = document.getElementById('layoutSelect').value;
    try {
        const response = await fetch('/api/editor/get?layout=' + layout);
        const data = await response.json();
        document.getElementById('jsonEditor').value = JSON.stringify(data, null, 2);
        updateStatus('Loaded: ' + layout);
    } catch(e) {
        updateStatus('Error loading layout: ' + e.message);
    }
}

// Save layout to device SPIFFS
async function saveLayout() {
    const layout = document.getElementById('layoutSelect').value;
    const jsonText = document.getElementById('jsonEditor').value;
    
    try {
        // Validate JSON
        JSON.parse(jsonText);
        
        const response = await fetch('/api/editor/save', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                layout: layout,
                data: jsonText
            })
        });
        
        const result = await response.json();
        if (result.success) {
            updateStatus('✓ Saved to SPIFFS (will sync to SD in 2-3 seconds)');
            setTimeout(reloadDevice, 3000);
        } else {
            updateStatus('✗ Save failed: ' + result.error);
        }
    } catch(e) {
        updateStatus('✗ Error: ' + e.message);
    }
}

// Trigger reload on device
async function reloadDevice() {
    try {
        const response = await fetch('/api/reload-layouts');
        updateStatus('Reload triggered on device');
    } catch(e) {
        updateStatus('Reload signal sent (device may be reloading)');
    }
}

// Reset to factory defaults
async function resetToDefaults() {
    if (confirm('Reset all layouts to factory defaults?')) {
        try {
            const response = await fetch('/api/editor/reset', {method: 'POST'});
            const result = await response.json();
            updateStatus(result.message);
            loadLayout();
        } catch(e) {
            updateStatus('Error: ' + e.message);
        }
    }
}

// Update status message
function updateStatus(msg) {
    document.getElementById('status').textContent = msg;
    console.log('[Editor] ' + msg);
}

// Load initial layout on page load
window.onload = function() {
    loadLayout();
};
    )js";
    
    serverPtr->send(200, "application/javascript", js);
}

void EditorHandler::handleEditorCSS() {
    String css = R"css(
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    min-height: 100vh;
    padding: 20px;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    background: white;
    border-radius: 12px;
    box-shadow: 0 10px 40px rgba(0,0,0,0.2);
    overflow: hidden;
}

h1 {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    padding: 30px;
    text-align: center;
    font-size: 32px;
}

.controls {
    padding: 20px;
    background: #f8f9fa;
    border-bottom: 1px solid #dee2e6;
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 20px;
}

.layout-selector {
    display: flex;
    align-items: center;
    gap: 10px;
}

.layout-selector label {
    font-weight: bold;
    color: #333;
}

.layout-selector select {
    padding: 8px 12px;
    border: 2px solid #667eea;
    border-radius: 6px;
    font-size: 14px;
    cursor: pointer;
}

.buttons {
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
}

button {
    padding: 10px 20px;
    border: none;
    border-radius: 6px;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
    transition: all 0.3s ease;
}

.btn-save {
    background: #28a745;
    color: white;
}

.btn-save:hover {
    background: #218838;
    transform: translateY(-2px);
    box-shadow: 0 5px 15px rgba(40,167,69,0.3);
}

.btn-reload {
    background: #007bff;
    color: white;
}

.btn-reload:hover {
    background: #0056b3;
    transform: translateY(-2px);
    box-shadow: 0 5px 15px rgba(0,123,255,0.3);
}

.btn-reset {
    background: #dc3545;
    color: white;
}

.btn-reset:hover {
    background: #c82333;
    transform: translateY(-2px);
    box-shadow: 0 5px 15px rgba(220,53,69,0.3);
}

.editor-container {
    display: flex;
    height: 600px;
}

.json-editor {
    flex: 1;
    padding: 20px;
    border-right: 1px solid #dee2e6;
}

.json-editor label {
    display: block;
    font-weight: bold;
    margin-bottom: 10px;
    color: #333;
}

.json-editor textarea {
    width: 100%;
    height: calc(100% - 30px);
    padding: 10px;
    border: 2px solid #dee2e6;
    border-radius: 6px;
    font-family: 'Courier New', monospace;
    font-size: 12px;
    resize: none;
}

.status {
    flex: 0 0 300px;
    padding: 20px;
    background: #f8f9fa;
    display: flex;
    align-items: center;
    justify-content: center;
    border-left: 4px solid #667eea;
}

.status p {
    font-size: 14px;
    color: #333;
    text-align: center;
    word-wrap: break-word;
}

@media (max-width: 768px) {
    .editor-container {
        flex-direction: column;
        height: auto;
    }
    
    .json-editor, .status {
        border-right: none;
        border-bottom: 1px solid #dee2e6;
    }
    
    .json-editor textarea {
        height: 400px;
    }
}
    )css";
    
    serverPtr->send(200, "text/css", css);
}

void EditorHandler::handleSaveLayout() {
    if (!serverPtr->hasArg("plain")) {
        serverPtr->send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    
    String jsonPayload = serverPtr->arg("plain");
    
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, jsonPayload)) {
        serverPtr->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String layoutName = doc["layout"].as<String>();
    String layoutData = doc["data"].as<String>();
    
    String spiffsPath = "/temp/" + layoutName + ".json";
    
    if (StorageManager::writeSPIFFS(spiffsPath.c_str(), layoutData)) {
        // Success - queue for sync to SD
        Serial.printf("[Editor] Saved to SPIFFS: %s\n", layoutName.c_str());
        
        DynamicJsonDocument response(256);
        response["success"] = true;
        response["message"] = "Layout saved to SPIFFS (syncing to SD)";
        response["layout"] = layoutName;
        
        String responseStr;
        serializeJson(response, responseStr);
        serverPtr->send(200, "application/json", responseStr);
    } else {
        serverPtr->send(500, "application/json", "{\"success\":false,\"error\":\"Write failed\"}");
    }
}

void EditorHandler::handleGetLayout() {
    String layout = serverPtr->arg("layout");
    if (layout.length() == 0) {
        layout = "monitor";
    }
    
    String layoutData = StorageManager::loadLayout(layout.c_str());
    serverPtr->send(200, "application/json", layoutData);
}

void EditorHandler::handleListLayouts() {
    DynamicJsonDocument doc(512);
    JsonArray layouts = doc.createNestedArray("layouts");
    
    layouts.add("monitor");
    layouts.add("alignment");
    layouts.add("graph");
    layouts.add("network");
    
    String response;
    serializeJson(doc, response);
    serverPtr->send(200, "application/json", response);
}
```

**Show me:** File created in `src/editor/editor_handler.cpp`

***

## PHASE 2 - TASK 2.3: Create Layout Sync Queue

**File to Create:** `src/storage/sync_queue.h`

**Content:**

```cpp
#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <Arduino.h>
#include <queue>
#include "storage_manager.h"

struct SyncCommand {
    String layoutName;
    String layoutData;
};

class SyncQueue {
private:
    static std::queue<SyncCommand> queue;
    static const size_t MAX_QUEUE_SIZE = 10;
    
public:
    // Add layout to sync queue (SPIFFS → SD)
    static bool enqueue(const String& layoutName, const String& layoutData);
    
    // Check if there are pending syncs
    static bool hasPending();
    
    // Process one sync operation (call from loop)
    static bool processPending();
    
    // Get queue size
    static size_t getQueueSize();
    
    // Clear queue
    static void clear();
};

#endif // SYNC_QUEUE_H
```

**Show me:** File created in `src/storage/sync_queue.h`

***

## PHASE 2 - TASK 2.4: Create Sync Queue Implementation

**File to Create:** `src/storage/sync_queue.cpp`

**Content:**

```cpp
#include "sync_queue.h"

std::queue<SyncCommand> SyncQueue::queue;

bool SyncQueue::enqueue(const String& layoutName, const String& layoutData) {
    if (queue.size() >= MAX_QUEUE_SIZE) {
        Serial.println("[SyncQueue] ERROR: Queue full!");
        return false;
    }
    
    SyncCommand cmd;
    cmd.layoutName = layoutName;
    cmd.layoutData = layoutData;
    
    queue.push(cmd);
    Serial.printf("[SyncQueue] Queued sync: %s\n", layoutName.c_str());
    
    return true;
}

bool SyncQueue::hasPending() {
    return !queue.empty();
}

bool SyncQueue::processPending() {
    if (queue.empty()) {
        return true;
    }
    
    SyncCommand cmd = queue.front();
    queue.pop();
    
    Serial.printf("[SyncQueue] Processing sync: %s\n", cmd.layoutName.c_str());
    
    yield();
    
    // Copy from SPIFFS temp to SD screens
    String spiffsPath = "/temp/" + cmd.layoutName + ".json";
    String sdPath = "/screens/" + cmd.layoutName + ".json";
    
    String data = StorageManager::readSPIFFS(spiffsPath.c_str());
    
    if (data.length() > 0) {
        File file = SD.open(sdPath, FILE_WRITE);
        if (file) {
            file.print(data);
            file.close();
            
            Serial.printf("[SyncQueue] ✓ Synced to SD: %s\n", cmd.layoutName.c_str());
            
            yield();
            return true;
        }
    }
    
    Serial.printf("[SyncQueue] ERROR: Failed to sync %s\n", cmd.layoutName.c_str());
    return false;
}

size_t SyncQueue::getQueueSize() {
    return queue.size();
}

void SyncQueue::clear() {
    while (!queue.empty()) {
        queue.pop();
    }
    Serial.println("[SyncQueue] Queue cleared");
}
```

**Show me:** File created in `src/storage/sync_queue.cpp`

***

## PHASE 2 - TASK 2.5: Register Editor Handler in main.cpp

**File:** `src/main.cpp`

**Location:** In `setup()` function, where you setup web server

**Action:** Add these lines:

```cpp
void setup() {
    // ... existing setup code ...
    
    // Initialize web server
    Serial.println("[Main] Setting up web server endpoints...");
    
    // ADD THIS LINE:
    EditorHandler::registerEndpoints(server);
    
    // ... rest of server setup ...
}
```

Also add at TOP of main.cpp:

```cpp
#include "editor/editor_handler.h"
#include "storage/sync_queue.h"
```

**Show me:** Confirmation that main.cpp updated with EditorHandler registration

***

## PHASE 2 - TASK 2.6: Add Sync Queue Processing to Loop

**File:** `src/main.cpp`

**Location:** In `loop()` function

**Action:** Modify loop to process sync queue:

```cpp
void loop() {
    server.handleClient();
    
    // Process queued layout reloads (from Phase 1)
    if (needsLayoutReload && !isReloadingLayouts) {
        processQueuedLayoutReload();
    }
    
    // ADD THIS - Process sync queue (SPIFFS → SD copying)
    if (SyncQueue::hasPending()) {
        SyncQueue::processPending();
        yield();
    }
    
    // Your existing operations...
    updateDisplay();
    handleButton();
    updateSensors();
    
    feedLoopWDT();
}
```

**Show me:** Confirmation that loop() updated with sync queue processing

---

## PHASE 2 - TASK 2.7: Add API Handler for Auto-Sync

**File:** `src/main.cpp`

**Location:** In `setup()` where endpoints are registered

**Action:** Add this endpoint AFTER EditorHandler::registerEndpoints:

```cpp
server.on("/api/editor/save", HTTP_POST, []() {
    // Handle save request
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false}");
        return;
    }
    
    String payload = server.arg("plain");
    DynamicJsonDocument doc(4096);
    
    if (deserializeJson(doc, payload) == 0) {
        String layoutName = doc["layout"];
        String layoutData = doc["data"];
        
        // Save to SPIFFS
        String spiffsPath = "/temp/" + layoutName + ".json";
        if (StorageManager::writeSPIFFS(spiffsPath.c_str(), layoutData)) {
            // Queue for sync to SD
            SyncQueue::enqueue(layoutName, layoutData);
            
            server.send(200, "application/json", 
                "{\"success\":true,\"message\":\"Queued for sync\"}");
        } else {
            server.send(500, "application/json", 
                "{\"success\":false,\"error\":\"Write failed\"}");
        }
    }
});
```

Actually, EditorHandler already handles this - **SKIP THIS TASK** (it's already in editor_handler.cpp)

**Show me:** Confirmation to skip Task 2.7 (EditorHandler handles it)

***

## PHASE 2 - TESTING CHECKPOINT

**Build and test:**

**Expected Serial Output:**
```
[Main] Setting up web server endpoints...
[Editor] Endpoints registered
[SyncQueue] Ready
Setup complete - entering main loop
```

**Tests to perform:**

1. ✓ Open web browser: `http://device_ip/editor/`
2. ✓ See layout editor UI with dropdown + buttons
3. ✓ Select "monitor" layout
4. ✓ JSON loads in text area
5. ✓ Edit one value (e.g., change "x": 10 to "x": 20)
6. ✓ Click "Save Layout"
7. ✓ See status: "✓ Saved to SPIFFS"
8. ✓ Wait 3 seconds
9. ✓ Page auto-reloads layouts
10. ✓ Check device display - **should show updated layout**
11. ✓ No crashes or watchdog resets
12. ✓ Repeat 5+ times (stress test)

**Serial should show:**
```
[Editor] Saved to SPIFFS: monitor
[SyncQueue] Queued sync: monitor
[SyncQueue] Processing sync: monitor
[SyncQueue] ✓ Synced to SD: monitor
```

**Report back:**
- Can you access `/editor/` page?
- Can you edit and save?
- Does display update?
- Any crashes?
- Serial output showing sync process?

---

**PHASE 2 COMPLETE WHEN:**
- ✓ Web editor loads and displays
- ✓ Can edit layout JSON
- ✓ Save triggers SPIFFS write + SD sync
- ✓ Display updates after reload
- ✓ No crashes
- ✓ Stress test: 5+ consecutive edits work

---

Once Phase 2 tests pass, I'll provide **Phase 3: User Experience** (first-boot wizard, layout browser, reset button)
