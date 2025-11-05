Here are the complete Claude Code instructions for implementing the safe queue-based upload system:

***

# SAFE SD UPLOAD QUEUE IMPLEMENTATION
## Claude Code Instructions for FluidDash-CYD

**Problem:** Direct SD access from web handlers causes spinlock crashes  
**Solution:** Queue-based architecture - handlers queue commands, loop() executes safely  
**Files Modified:** 3 files (header, implementation, main)  
**Estimated Time:** 1-2 hours  

---

## TASK 1: Create Upload Queue Header

**File to Create:** `src/upload_queue.h`

**Instructions:**

Create a new file with this exact content:

```cpp
#ifndef UPLOAD_QUEUE_H
#define UPLOAD_QUEUE_H

#include <Arduino.h>
#include <queue>
#include <string>

// Define max upload size (adjust based on your needs)
#define MAX_UPLOAD_SIZE 8192  // 8KB chunks

struct UploadCommand {
    String filename;
    String data;
    bool isQueued;
};

class SDUploadQueue {
private:
    std::queue<UploadCommand> cmdQueue;
    static const int MAX_QUEUE_SIZE = 10;
    
public:
    SDUploadQueue() {}
    
    // Add command to queue
    bool enqueue(const String& filename, const String& data) {
        if (cmdQueue.size() >= MAX_QUEUE_SIZE) {
            Serial.println("[Queue] ERROR: Upload queue full!");
            return false;
        }
        
        UploadCommand cmd;
        cmd.filename = filename;
        cmd.data = data;
        cmd.isQueued = true;
        
        cmdQueue.push(cmd);
        Serial.printf("[Queue] Upload queued: %s (%d bytes)\n", 
                      filename.c_str(), data.length());
        return true;
    }
    
    // Check if commands pending
    bool hasPending() {
        return !cmdQueue.empty();
    }
    
    // Get next command (does NOT remove from queue)
    UploadCommand peek() {
        if (cmdQueue.empty()) {
            return {"", "", false};
        }
        return cmdQueue.front();
    }
    
    // Remove command after successful processing
    void dequeue() {
        if (!cmdQueue.empty()) {
            cmdQueue.pop();
        }
    }
    
    // Get queue size
    int size() {
        return cmdQueue.size();
    }
    
    // Clear queue
    void clear() {
        while (!cmdQueue.empty()) {
            cmdQueue.pop();
        }
    }
};

#endif // UPLOAD_QUEUE_H
```

**Verification:** File created with no errors shown

***

## TASK 2: Create Upload Queue Implementation

**File to Create:** `src/upload_queue.cpp`

**Instructions:**

Create a new file with this exact content (minimal implementation):

```cpp
#include "upload_queue.h"

// Queue is implemented entirely in header as inline functions
// This file exists for future expansion
// No additional code needed here
```

**Verification:** File created

***

## TASK 3: Create Global Queue in main.cpp

**File:** `src/main.cpp`  
**Action:** Add queue declaration at top level

**Instructions:**

1. Find the section with global variable declarations (near top of file)
2. Add this line AFTER other global declarations:

```cpp
// Upload queue - handlers queue uploads, loop() executes them
SDUploadQueue uploadQueue;
```

3. Also add the include at top of main.cpp with other includes:

```cpp
#include "upload_queue.h"
```

4. Show me the global declarations section with the new line added

**Verification:** Confirm queue declared globally

***

## TASK 4: Modify handleUploadJSON Handler

**File:** `src/webserver_manager.cpp`  
**Function:** `handleUploadJSON()` (or similar upload handler)

**Instructions:**

Find the current handleUploadJSON() function. Replace it with this:

```cpp
void handleUploadJSON() {
    // Validate request
    if (!server.hasArg("filename") || !server.hasArg("data")) {
        server.send(400, "application/json", "{\"error\":\"Missing filename or data\"}");
        return;
    }
    
    String filename = server.arg("filename");
    String data = server.arg("data");
    
    // Validate filename (security)
    if (filename.length() == 0 || filename.length() > 255) {
        server.send(400, "application/json", "{\"error\":\"Invalid filename\"}");
        return;
    }
    
    // Ensure .json extension
    if (!filename.endsWith(".json")) {
        filename += ".json";
    }
    
    // Validate data size
    if (data.length() > MAX_UPLOAD_SIZE) {
        server.send(413, "application/json", 
                   "{\"error\":\"File too large\"}");
        return;
    }
    
    // Queue the upload - do NOT execute here
    if (uploadQueue.enqueue(filename, data)) {
        server.send(200, "application/json", 
                   "{\"status\":\"Upload queued\",\"filename\":\"" + filename + "\"}");
    } else {
        server.send(503, "application/json", 
                   "{\"error\":\"Upload queue full, try again\"}");
    }
}
```

**Key Changes:**
- ✗ NO direct SD.open() or file writes
- ✓ ONLY validates input
- ✓ ONLY queues the command
- ✓ Returns immediately to caller

**Show me:** The complete modified function

***

## TASK 5: Add Process Upload Function

**File:** `src/webserver_manager.cpp`  
**Action:** Create new helper function

**Instructions:**

Add this new function to webserver_manager.cpp:

```cpp
// Process queued uploads - called from main loop only
bool processQueuedUpload() {
    // Check if there's a pending upload
    if (!uploadQueue.hasPending()) {
        return true;
    }
    
    // Get (but don't remove yet) the command
    UploadCommand cmd = uploadQueue.peek();
    
    Serial.printf("[Upload] Processing: %s (%d bytes)\n", 
                  cmd.filename.c_str(), cmd.data.length());
    
    // Safe SD access - only called from loop()
    try {
        // Ensure directory exists
        if (!SD.exists("/screens")) {
            if (!SD.mkdir("/screens")) {
                Serial.println("[Upload] ERROR: Failed to create /screens directory");
                uploadQueue.dequeue();  // Remove failed command
                return false;
            }
        }
        
        // Write file
        File file = SD.open("/screens/" + cmd.filename, FILE_WRITE);
        if (!file) {
            Serial.printf("[Upload] ERROR: Failed to open file for writing: %s\n", 
                         cmd.filename.c_str());
            uploadQueue.dequeue();  // Remove failed command
            return false;
        }
        
        size_t bytesWritten = file.print(cmd.data);
        file.close();
        
        if (bytesWritten == cmd.data.length()) {
            Serial.printf("[Upload] SUCCESS: %s (%d bytes written)\n", 
                         cmd.filename.c_str(), bytesWritten);
            uploadQueue.dequeue();  // Remove successful command
            return true;
        } else {
            Serial.printf("[Upload] ERROR: Partial write - expected %d, got %d\n", 
                         cmd.data.length(), bytesWritten);
            uploadQueue.dequeue();  // Remove failed command
            return false;
        }
        
    } catch (const std::exception& e) {
        Serial.printf("[Upload] EXCEPTION: %s\n", e.what());
        uploadQueue.dequeue();  // Remove failed command
        return false;
    }
}
```

**Show me:** Confirmation that function added

***

## TASK 6: Modify loop() to Process Queue

**File:** `src/main.cpp`  
**Location:** In loop() function

**Instructions:**

Find the loop() function. Modify it to process uploads safely:

```cpp
void loop() {
    // Handle web requests
    server.handleClient();
    
    // Process one queued upload per loop iteration (safe SD access)
    // This prevents blocking and spreads processing over time
    if (uploadQueue.hasPending()) {
        processQueuedUpload();
        yield();  // Feed watchdog during upload processing
    }
    
    // Your existing operations continue...
    updateDisplay();
    handleButton();
    updateSensors();
    
    // Feed watchdog
    feedLoopWDT();
}
```

**Key Points:**
- ✓ `server.handleClient()` stays at top (quick web requests)
- ✓ Upload processing comes next (one per loop cycle)
- ✓ `yield()` after upload processing
- ✓ Rest of loop continues normally

**Show me:** The modified loop() function with upload queue processing

***

## TASK 7: Add Queue Status Handler (Optional)

**File:** `src/webserver_manager.cpp`  
**Action:** Create diagnostic handler

**Instructions:**

Add this handler to monitor queue:

```cpp
void handleUploadStatus() {
    // Return status of upload queue
    DynamicJsonDocument doc(256);
    doc["queueSize"] = uploadQueue.size();
    doc["hasPending"] = uploadQueue.hasPending();
    
    if (uploadQueue.hasPending()) {
        UploadCommand next = uploadQueue.peek();
        doc["nextFilename"] = next.filename;
        doc["nextSize"] = next.data.length();
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
```

**Register in setup():**
```cpp
server.on("/api/upload-status", HTTP_GET, handleUploadStatus);
```

**Show me:** Handler added and registered

***

## VERIFICATION CHECKLIST

After all tasks complete:

- [ ] `src/upload_queue.h` created
- [ ] `src/upload_queue.cpp` created
- [ ] `upload_queue.h` included in main.cpp
- [ ] `SDUploadQueue uploadQueue;` declared in main.cpp
- [ ] `handleUploadJSON()` modified - NO direct SD access
- [ ] `processQueuedUpload()` added to webserver_manager.cpp
- [ ] `loop()` modified to call `processQueuedUpload()`
- [ ] Code shows complete (ready for build)

---

## BUILD & TEST

After you build:

**Test sequence:**
1. Device boots - no crashes ✓
2. `curl http://device/api/upload-status` → `{"queueSize":0,"hasPending":false}`
3. Queue an upload: `curl -X POST http://device/api/upload -d "filename=test.json&data={...}"`
4. Should return: `{"status":"Upload queued","filename":"test.json"}`
5. Check queue status again - should process within 1-2 loop cycles
6. No spinlock crashes ✓

---

## IF BUILD FAILS

Post only the error messages. Most likely issues:

- **"uploadQueue undeclared"** → Make sure declared in main.cpp
- **"upload_queue.h not found"** → Verify file created in src/
- **"processQueuedUpload undeclared"** → Function must be in webserver_manager.cpp

***

## KEY IMPROVEMENTS

✅ **Safety:** Web handlers don't touch SD anymore  
✅ **Stability:** Upload processing only in main loop context  
✅ **Responsiveness:** Web requests return immediately  
✅ **Diagnostics:** Can check queue status anytime  
✅ **Scalability:** Can handle multiple queued uploads  

***

**Ready to proceed?** Tell Claude Code to start with TASK 1.