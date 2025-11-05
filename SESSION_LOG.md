Perfect! Now let me update the todo list and create a summary:
Update Todos

Pre-flight checks - verify file structure

SESSION 1: Configuration & Includes (Tasks 1.1-1.3)

SESSION 2: Convert 17 web handlers to WebServer pattern

SESSION 3: Setup & Loop Integration

SESSION 4: Test and verify
✓ MIGRATION COMPLETE
Summary of Changes:
SESSION 1: Configuration & Includes
✓ Removed AsyncWebServer and AsyncTCP from platformio.ini
✓ Changed #include <ESPAsyncWebServer.h> → #include <WebServer.h> in main.cpp
✓ Changed AsyncWebServer server(80) → WebServer server(80) in main.cpp
SESSION 2: Handler Conversion
Created 17 WebServer-compatible handler functions:
handleRoot(), handleSettings(), handleAdmin(), handleWiFi()
handleAPIConfig(), handleAPIStatus(), handleAPISave(), handleAPIAdminSave()
handleAPIResetWiFi(), handleAPIRestart(), handleAPIWiFiConnect()
handleAPIReloadScreens(), handleUpload(), handleUploadJSON()
handleGetJSON(), handleSaveJSON(), handleEditor()
Key conversions:
request->send() → server.send()
request->hasParam() → server.hasArg()
request->getParam()->value() → server.arg()
Lambda functions → Named functions with no parameters
SESSION 3: Setup & Loop Integration
✓ Replaced all lambda handlers in setupWebServer() with function pointer registrations
✓ Added server.handleClient() at start of loop()
Files Modified:
platformio.ini - Dependencies updated
src/main.cpp - Complete AsyncWebServer → WebServer migration
Ready for Build: All code changes complete. Please build locally and report any errors.

Build success. Upload success.
Problem: Boot loop

---

## Session 1 - AsyncWebServer → WebServer Migration
**Date:** 2025-01-05
**Branch:** fix-sd-web-access

### Tasks Completed
- [x] Pre-flight checks - verified project structure
- [x] Updated platformio.ini - removed AsyncWebServer/AsyncTCP deps
- [x] Changed includes: ESPAsyncWebServer → WebServer
- [x] Changed server declaration: AsyncWebServer → WebServer
- [x] Created 17 handler functions (lambda → named functions)
- [x] Updated setupWebServer() - replaced lambdas with function pointers
- [x] Added server.handleClient() to loop()

### Files Modified
- `platformio.ini` (lines 18-26)
- `src/main.cpp` (lines 52, 62, 820-822, 888-1115)

### Handlers Converted (17 total)
GET: /, /settings, /admin, /wifi, /api/config, /api/status, /upload, /get-json, /editor
POST: /api/save, /api/admin/save, /api/reset-wifi, /api/restart, /api/wifi/connect, /api/reload-screens, /upload-json, /save-json

### Issues Encountered
- Boot loop after upload

### Next Session
- [ ] Debug boot loop - check serial output
- [ ] Verify handler signatures
- [ ] Test web endpoints if boot succeeds

---

## Session 2 - Watchdog Fix
**Date:** 2025-01-05
**Branch:** fix-sd-web-access

### Issue
Watchdog timeout - server.handleClient() blocked feedLoopWDT()

### Tasks Completed
- [x] Added yield() to handleAPIReloadScreens (4 calls after SD reads)
- [x] Added yield() to handleUploadJSON (4 calls around SD operations)
- [x] Added yield() to handleSaveJSON (3 calls around SD operations)

### Files Modified
- `src/main.cpp` (lines 1027-1033, 1079-1109, 1135-1144)

### Handlers Modified (3 of 17)
1. handleAPIReloadScreens - yield after each loadScreenConfig
2. handleUploadJSON - yield after SD open/write/close
3. handleSaveJSON - yield after SD open/print/close

### Git Checkpoint
**NOW:** Commit with message:
```
Fix watchdog timeout - add yield() to SD handlers

- handleAPIReloadScreens: yield after each screen load
- handleUploadJSON: yield after SD operations
- handleSaveJSON: yield after SD operations
```

### Result
✓ Build/upload successful
✓ No boot loop - watchdog fixed

### Next Steps
- [ ] Test web interface endpoints
- [ ] Verify SD card access from web
- [ ] Check for stability over time

---

## Session 3 - SD Card Mutex Fix
**Date:** 2025-01-05
**Branch:** fix-sd-web-access

### Issue
Spinlock crash during file upload - loop() and web handler accessing SD simultaneously

### Tasks Completed
- [x] Added SD mutex (SemaphoreHandle_t)
- [x] Initialize mutex in setup()
- [x] Wrapped handleUploadJSON with mutex
- [x] Wrapped handleAPIReloadScreens with mutex
- [x] Wrapped handleSaveJSON with mutex

### Files Modified
- `src/main.cpp` (lines 59, 601, 1029-1041, 1082-1125, 1145-1158)

### Git Checkpoint
Commit message:
```
Add SD card mutex for synchronous WebServer

- Prevents spinlock crash when web handlers access SD
- Mutex protects: upload, reload-screens, save-json handlers
```

### Result
❌ Mutex approach failed - spinlock crash persisted
- Error: `assert failed: xQueueSemaphoreTake queue.c:1549`
- Root cause: Can't use FreeRTOS mutexes from web handler context
- Understanding: WebServer handlers run in different execution context

### Next Session
- [ ] Implement queue-based architecture (no mutex)
- [ ] Web handlers queue operations, loop() executes them

---

## Session 4 - Queue-Based Upload System
**Date:** 2025-01-05
**Branch:** fix-sd-web-access

### Issue
Mutex approach failed - need architectural change to separate web response from SD operations

### Solution Architecture
Queue-based system:
- Web handlers validate input and queue operations → return immediately
- Main loop() processes queued operations with safe SD access
- No mutex needed - single-threaded SD access only
- Eliminates spinlock and semaphore issues

### Tasks Completed
- [x] Created upload_queue.h with SDUploadQueue class
- [x] Created upload_queue.cpp (minimal implementation)
- [x] Added uploadQueue global variable to main.cpp
- [x] Modified handleUploadJSON - removed direct SD access, added queueing
- [x] Created processQueuedUpload() function for safe SD operations
- [x] Modified loop() to process queued uploads
- [x] Added handleUploadStatus() diagnostic endpoint
- [x] Registered new handlers in setup()

### Files Created
- `src/upload_queue.h` - Queue class definition
- `src/upload_queue.cpp` - Implementation file

### Files Modified
- `src/main.cpp`:
  - Line 57: Added `#include "upload_queue.h"`
  - Line 63: Declared `SDUploadQueue uploadQueue;`
  - Lines 1082-1118: Replaced handleUploadJSON (queue-based, no SD access)
  - Lines 1127-1142: Added handleUploadStatus() diagnostic handler
  - Lines 1145-1187: Added processQueuedUpload() function
  - Lines 836-839: Modified loop() to process upload queue
  - Lines 1257, 1259: Registered handlers in setup()

### Key Implementation Details

**handleUploadJSON() - Web Handler (No SD Access):**
- Validates filename and data parameters
- Checks data size against MAX_UPLOAD_SIZE (8KB)
- Queues upload command
- Returns immediately with success/error JSON

**processQueuedUpload() - Loop Processing (Safe SD Access):**
- Called only from main loop() context
- Checks queue with hasPending()
- Peeks at next command (doesn't remove yet)
- Performs SD operations with proper error handling
- Dequeues command after completion (success or failure)

**loop() Integration:**
```cpp
if (uploadQueue.hasPending()) {
  processQueuedUpload();
  yield();  // Feed watchdog
}
```

### New Endpoints
- `GET /api/upload-status` - Returns queue status, pending uploads
- `POST /upload-json` - Queue-based upload (changed from file upload to POST params)

### Git Checkpoint
Commit message:
```
Add queue-based upload system for safe SD access

- Web handlers queue uploads, loop() processes them
- Eliminates spinlock crashes from concurrent SD access
- Removed mutex approach - single-threaded architecture
- Added upload status diagnostic endpoint
- handleUploadJSON now accepts filename/data POST parameters
```

### Ready for Testing
Test sequence:
1. Boot device - verify no crashes
2. `curl http://device/api/upload-status` → Check queue empty
3. `curl -X POST http://device/upload-json -d "filename=test.json&data={...}"`
4. Should return: `{"status":"Upload queued","filename":"test.json"}`
5. Monitor serial output for upload processing
6. Verify no spinlock or semaphore crashes

### Build & Test Results - Session 4A
- ✓ Build successful
- ✓ Upload successful
- ✓ Boot without crashes
- ✓ Web interface loads
- ❌ Upload still crashes device

### Issue Analysis
- Upload handler works (queues data)
- Crash occurs during processQueuedUpload() execution
- Multiple SD library issues identified:
  1. No yield() calls during write
  2. String concatenation during SD I/O
  3. No settling time between operations
  4. Using print() instead of write()
  5. No retry logic

---

## Session 4B - Mutex Removal & Layout Queue
**Date:** 2025-01-05 (continued)
**Branch:** fix-sd-web-access

### Issue
Upload crash persisted. Root cause: SD library internal mutex triggered by direct access patterns.

### Tasks Completed

**Mutex Cleanup:**
- [x] Searched entire project for remaining mutex code
- [x] Removed global sdMutex declaration (line 60)
- [x] Removed mutex initialization in setup() (line 603)
- [x] Cleaned handleAPIReloadScreens() - removed mutex calls
- [x] Cleaned handleSaveJSON() - removed mutex calls
- [x] Verified zero mutex/semaphore references remain

**Layout Reload Queue Implementation:**
- [x] Added global flags (needsLayoutReload, isReloadingLayouts)
- [x] Modified handleAPIReloadScreens() to queue only
- [x] Created processQueuedLayoutReload() function
- [x] Modified loop() to process layout queue
- [x] Verified loadScreenConfig() only in setup() and queue processor

**Hardened SD Write Implementation:**
- [x] Added yield()+delay() before all SD operations
- [x] Pre-build file paths to prevent string allocation during I/O
- [x] Added retry logic for file open (3 attempts)
- [x] Changed to chunked writes (512 bytes) with yields
- [x] Changed from print() to write() for binary safety
- [x] Added settling delays after file close
- [x] Added cleanup on failure (delete incomplete files)

### Files Modified - Session 4B

**src/main.cpp:**
- Lines 62-64: Added layout queue flags
- Line 1033-1055: handleAPIReloadScreens() - queue only, no SD access
- Lines 829-877: Added processQueuedLayoutReload()
- Lines 883-886: Modified loop() to process layout queue
- Lines 1217-1344: Hardened processQueuedUpload() with:
  - 15+ yield() calls
  - Pre-built paths
  - Retry logic
  - Chunked writes
  - Settling delays

### Key Architectural Changes

**Single-Context SD Access:**
- ✓ Web handlers NEVER touch SD directly
- ✓ All SD operations happen in loop() context only
- ✓ Upload queue: handlers queue → loop processes
- ✓ Layout queue: handlers queue → loop processes
- ✓ No mutex needed - sequential access only

**Hardened SD Write Pattern:**
```cpp
yield() + delay(5ms)  // Pre-operation settling
String path = build_path();  // Before I/O
File file = SD.open() with retries
for (each 512-byte chunk) {
    file.write();
    yield();  // After each chunk
}
file.close();
yield() + delay(10ms)  // Post-operation settling
```

### Current Status
- All code complete and building successfully
- Upload still crashes (SD library internal issues)
- Ready for git checkpoint before major refactoring

### Next Steps
- [ ] Git commit current stable state
- [ ] Consider alternative approaches:
  - Disable problematic endpoints temporarily
  - Investigate ESP32 SD library configuration
  - Test with different SD card
  - Consider SPIFFS instead of SD for uploads