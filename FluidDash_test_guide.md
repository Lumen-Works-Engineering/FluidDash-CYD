FluidDash Testing Guide
Web Pages

1. Main Dashboard - http://192.168.73.156/
   What to test: Main interface with live data
   Expected: Display shows temperatures, FluidNC status, coordinates
   Check: Real-time updates every 2 seconds
2. Upload Page - http://192.168.73.156/upload
   What to test: JSON layout file upload
   How to test:
   Click "Choose File" and select a .json file
   Click "Upload to SPIFFS"
   Should see success message
   Click "Reboot Device Now" button
   Device reboots and loads new layout
   Expected: Green success message, reboot button appears
3. Admin/Calibration Page - http://192.168.73.156/admin
   What to test: Temperature calibration, PSU calibration, RTC settings
   How to test:
   Temperature Calibration:
   Enter offset values (e.g., 0.5, -0.3, etc.)
   Click "Save Calibration"
   Check for success message
   RTC Time Setting:
   Click "Use Browser Time" (auto-fills current time)
   OR manually select date/time
   Click "Set RTC Time"
   Check current RTC time updates
   Expected: Real-time readings display, success messages show, RTC time updates
4. WiFi Configuration - http://192.168.73.156/wifi
   What to test: WiFi network management
   How to test:
   View current connection status
   Enter new SSID/password (optional)
   Click "Connect to WiFi"
   Expected: Shows current WiFi status, IP address
   API Endpoints - Status & Data
   GET /api/status
   curl http://192.168.73.156/api/status
   Expected Response:
   {
   "machine_state":"IDLE",
   "temperatures":[23.50,24.10,23.80,24.30],
   "fan_speed":128,
   "fan_rpm":2450,
   "psu_voltage":24.12,
   "wpos_x":0.000,"wpos_y":0.000,"wpos_z":0.000,
   "mpos_x":0.000,"mpos_y":0.000,"mpos_z":0.000,
   "connected":true
   }
   GET /api/config
   curl http://192.168.73.156/api/config
   Expected Response:
   {
   "device_name":"FluidDash",
   "fluidnc_ip":"192.168.73.14",
   "temp_low":20.0,
   "temp_high":50.0,
   "fan_min":30,
   "psu_low":23.0,
   "psu_high":25.0,
   "graph_time":300,
   "graph_interval":5
   }
   GET /api/upload-status
   curl http://192.168.73.156/api/upload-status
   Expected Response:
   {
   "spiffs_available":true,
   "sd_available":true
   }
   API Endpoints - RTC (NEW)
   GET /api/rtc - Get current RTC time
   curl http://192.168.73.156/api/rtc
   Expected Response:
   {
   "success":true,
   "timestamp":"2025-01-05 14:35:22"
   }
   POST /api/rtc/set - Set RTC time
   curl -X POST http://192.168.73.156/api/rtc/set \
   -d "date=2025-01-05&time=14:35:30"
   Expected Response:
   {
   "success":true,
   "message":"RTC time updated successfully"
   }
   Serial Output:
   [RTC] Time set to: 2025-01-05 14:35:30
   API Endpoints - Configuration
   POST /api/save - Save user settings
   curl -X POST http://192.168.73.156/api/save \
   -d "temp_low=18.0&temp_high=55.0&fan_min=25&psu_low=23.5&psu_high=24.5"
   Expected Response:
   Settings saved successfully
   POST /api/admin/save - Save calibration settings
   curl -X POST http://192.168.73.156/api/admin/save \
   -d "cal_x=0.5&cal_yl=-0.3&cal_yr=0.2&cal_z=-0.1&psu_cal=6.850"
   Expected Response:
   Calibration saved successfully
   API Endpoints - System Control
   GET /api/reboot - Reboot device
   curl http://192.168.73.156/api/reboot
   Expected Response:
   {
   "status":"Rebooting device...",
   "message":"Device will restart in 1 second"
   }
   Result: Device reboots after 1 second POST /api/restart - Restart device
   curl -X POST http://192.168.73.156/api/restart
   Expected Response:
   Restarting device...
   Result: Device restarts after 1 second POST /api/reload-screens - Reload layouts (triggers reboot)
   curl -X POST http://192.168.73.156/api/reload-screens
   Expected Response:
   {
   "status":"Rebooting device to load new layouts...",
   "message":"Device will restart in 1 second"
   }
   POST /api/reset-wifi - Reset WiFi settings
   curl -X POST http://192.168.73.156/api/reset-wifi
   Expected Response:
   WiFi settings cleared. Device will restart...
   API Endpoints - File Upload
   POST /upload-json - Upload layout file
   curl -X POST http://192.168.73.156/upload-json \
   -F "file=@monitor.json"
   Expected Response:
   {
   "success":true
   }
   Serial Output:
   [Upload] Starting: monitor.json
   [Upload] Saving 7219 bytes to SPIFFS: /screens/monitor.json
   [StorageMgr] Wrote 7219 bytes to /screens/monitor.json
   [Upload] SUCCESS: Saved to SPIFFS
   GET /get-json - Download layout file
   curl "http://192.168.73.156/get-json?file=monitor.json"
   Expected: Returns JSON file contents POST /save-json - Save edited layout
   curl -X POST http://192.168.73.156/save-json \
   -d "filename=test.json&data={...json content...}"
   Expected Response:
   Saved successfully
   Testing Checklist
   Basic Functionality:
   Main page loads and shows live data
   Upload page accepts .json files
   Admin page displays current readings
   WiFi page shows connection status
   RTC section shows current time
   RTC Feature (NEW):
   Current RTC time displays on admin page
   "Use Browser Time" button populates fields
   Manual time entry works
   RTC time updates after submit
   Success message appears
   Serial shows: [RTC] Time set to: YYYY-MM-DD HH:MM:SS
   File Operations:
   Upload .json file → success message
   Reboot button appears after upload
   Click reboot → device restarts
   New layout loads on boot
   No crashes during upload/reboot cycle
   API Endpoints:
   /api/status returns live sensor data
   /api/config returns current settings
   /api/rtc returns current RTC time
   /api/rtc/set updates RTC successfully
   /api/save saves user settings
   /api/admin/save saves calibration
   /api/reboot triggers clean reboot
   System Stability:
   No crashes or watchdog resets
   FluidNC connection stays connected
   Display updates smoothly
   Web interface responsive
   Multiple consecutive uploads work
   Serial Monitor Commands to Watch
   During testing, monitor serial output for these key messages: Successful Operations:
   [Upload] SUCCESS: Saved to SPIFFS
   [StorageMgr] Wrote XXXX bytes to /screens/filename.json
   [RTC] Time set to: YYYY-MM-DD HH:MM:SS
   [FluidNC] Connected to: /ws
   [JSON] Loaded XX elements from FluidDash
   Errors to Watch For:
   [Upload] ERROR: ...
   [StorageMgr] ERROR: ...
   [FluidNC] Disconnected!
   assert failed: xQueueSemaphoreTake
   Watchdog timeout
   Everything should work without crashes! Let me know which features you'd like to test first or if you encounter any issues.