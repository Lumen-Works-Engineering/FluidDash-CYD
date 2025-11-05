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

### Next
- [ ] Rebuild/upload
- [ ] Test file upload without crash