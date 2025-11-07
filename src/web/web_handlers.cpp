#include "web_handlers.h"
#include "web_api.h"
#include "config/config.h"
#include "state/system_state.h"
#include "state/fluidnc_state.h"
#include "storage_manager.h"
#include "utils/utils.h"
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <RTClib.h>
#include <SD.h>
#include <ArduinoJson.h>

// External references
extern WebServer server;
extern WiFiManager wm;
extern Preferences prefs;
extern RTC_DS3231 rtc;
extern StorageManager storage;

// Forward declarations for HTML getters (will be in html_pages module later)
extern String getMainHTML();
extern String getSettingsHTML();
extern String getAdminHTML();
extern String getWiFiConfigHTML();

// ========== Page Handlers ==========

void handleRoot()
{
    server.send(200, "text/html", getMainHTML());
}

void handleSettings()
{
    server.send(200, "text/html", getSettingsHTML());
}

void handleAdmin()
{
    server.send(200, "text/html", getAdminHTML());
}

void handleWiFi()
{
    server.send(200, "text/html", getWiFiConfigHTML());
}

void handleUpload()
{
    String html = "<!DOCTYPE html><html><head><title>Upload JSON</title>";
    html += "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff}";
    html += "h1{color:#00bfff}.box{background:#2a2a2a;padding:20px;border-radius:8px;max-width:600px}";
    html += "button{background:#00bfff;color:#000;padding:10px 20px;border:none;cursor:pointer;margin:5px}";
    html += "#status{margin-top:20px;padding:10px}.success{background:#004d00;color:#0f0}";
    html += ".error{background:#4d0000;color:#f00}.info{background:#003d5c;color:#00bfff}";
    html += ".note{background:#2a2a2a;padding:10px;margin:10px 0;border-left:3px solid #00bfff}</style></head><body>";
    html += "<h1>Upload JSON Layout</h1><div class='box'>";
    html += "<div class='note'><strong>Note:</strong> After upload, device must reboot to load new layouts.</div>";
    html += "<h3>Upload Screen Layout</h3>";
    html += "<form id='f' enctype='multipart/form-data'>";
    html += "<input type='file' id='file' accept='.json' required><br><br>";
    html += "<button type='submit'>Upload to SPIFFS</button></form>";
    html += "<div id='status'></div>";
    html += "<div id='reboot' style='display:none;margin-top:10px'>";
    html += "<button onclick='reboot()'>Reboot Device Now</button></div></div>";
    html += "<script>function reboot(){document.getElementById('status').innerHTML='Rebooting...';";
    html += "document.getElementById('status').className='info';fetch('/api/reboot').then(()=>{";
    html += "setTimeout(()=>{window.location.href='/'},3000)});}";
    html += "document.getElementById('f').addEventListener('submit',function(e){";
    html += "e.preventDefault();let file=document.getElementById('file').files[0];";
    html += "if(!file)return;let s=document.getElementById('status');";
    html += "s.innerHTML='Uploading to SPIFFS...';s.className='info';let fd=new FormData();";
    html += "fd.append('file',file);fetch('/upload-json',{method:'POST',body:fd})";
    html += ".then(r=>r.json()).then(d=>{if(d.success){";
    html += "s.innerHTML='Upload successful! File saved to SPIFFS. Click button to reboot and load new layouts.';";
    html += "s.className='success';document.getElementById('reboot').style.display='block'}";
    html += "else{s.innerHTML='Upload failed';s.className='error'}})";
    html += ".catch(e=>{s.innerHTML='Upload failed';s.className='error'})});</script>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleEditor()
{
    // Serve editor from filesystem (no memory issues)
    File file = LittleFS.open("/editor.html", "r");
    
    if (!file) {
        String html = "<!DOCTYPE html><html><head><title>Editor Not Found</title></head><body>";
        html += "<h1>Editor file not found</h1>";
        html += "<p>Make sure to upload filesystem: <code>pio run --target uploadfs</code></p>";
        html += "<p><a href='/'>Back to Dashboard</a></p>";
        html += "</body></html>";
        server.send(404, "text/html", html);
        return;
    }
    
    server.streamFile(file, "text/html");
    file.close();
}

// ========== API Handlers (GET) ==========

void handleAPIConfig()
{
    server.send(200, "application/json", getConfigJSON());
}

void handleAPIStatus()
{
    server.send(200, "application/json", getStatusJSON());
}

void handleAPIRTC()
{
    server.send(200, "application/json", getRTCJSON());
}

void handleUploadStatus()
{
    server.send(200, "application/json", getUploadStatusJSON());
}

void handleGetJSON()
{
    // DISABLED - was causing crashes with SD card access
    server.send(503, "application/json", "{\"success\":false,\"message\":\"Endpoint disabled - causing crashes\"}");
}

// ========== API Handlers (POST) ==========

void handleAPISave()
{
    // Update config from POST parameters
    if (server.hasArg("temp_low"))
    {
        cfg.temp_threshold_low = server.arg("temp_low").toFloat();
    }
    if (server.hasArg("temp_high"))
    {
        cfg.temp_threshold_high = server.arg("temp_high").toFloat();
    }
    if (server.hasArg("fan_min"))
    {
        cfg.fan_min_speed = server.arg("fan_min").toInt();
    }
    if (server.hasArg("graph_time"))
    {
        uint16_t newTime = server.arg("graph_time").toInt();
        if (newTime != cfg.graph_timespan_seconds)
        {
            cfg.graph_timespan_seconds = newTime;
            allocateHistoryBuffer(); // Reallocate with new size
        }
    }
    if (server.hasArg("graph_interval"))
    {
        cfg.graph_update_interval = server.arg("graph_interval").toInt();
    }
    if (server.hasArg("psu_low"))
    {
        cfg.psu_alert_low = server.arg("psu_low").toFloat();
    }
    if (server.hasArg("psu_high"))
    {
        cfg.psu_alert_high = server.arg("psu_high").toFloat();
    }
    if (server.hasArg("coord_decimals"))
    {
        cfg.coord_decimal_places = server.arg("coord_decimals").toInt();
    }

    saveConfig();
    server.send(200, "text/plain", "Settings saved successfully");
}

void handleAPIAdminSave()
{
    if (server.hasArg("cal_x"))
    {
        cfg.temp_offset_x = server.arg("cal_x").toFloat();
    }
    if (server.hasArg("cal_yl"))
    {
        cfg.temp_offset_yl = server.arg("cal_yl").toFloat();
    }
    if (server.hasArg("cal_yr"))
    {
        cfg.temp_offset_yr = server.arg("cal_yr").toFloat();
    }
    if (server.hasArg("cal_z"))
    {
        cfg.temp_offset_z = server.arg("cal_z").toFloat();
    }
    if (server.hasArg("psu_cal"))
    {
        cfg.psu_voltage_cal = server.arg("psu_cal").toFloat();
    }

    saveConfig();
    server.send(200, "text/plain", "Calibration saved successfully");
}

void handleAPIResetWiFi()
{
    wm.resetSettings();
    server.send(200, "text/plain", "WiFi settings cleared. Device will restart...");
    delay(1000);
    ESP.restart();
}

void handleAPIRestart()
{
    server.send(200, "text/plain", "Restarting device...");
    delay(1000);
    ESP.restart();
}

void handleAPIReboot()
{
    server.send(200, "application/json",
                "{\"status\":\"Rebooting device...\",\"message\":\"Device will restart in 1 second\"}");
    delay(1000);
    ESP.restart();
}

void handleAPIWiFiConnect()
{
    String ssid = "";
    String password = "";

    if (server.hasArg("ssid"))
    {
        ssid = server.arg("ssid");
    }
    if (server.hasArg("password"))
    {
        password = server.arg("password");
    }

    if (ssid.length() == 0)
    {
        server.send(200, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
        return;
    }

    Serial.println("Attempting to connect to: " + ssid);

    // Store credentials in preferences
    prefs.begin("fluiddash", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", password);
    prefs.end();

    // Send response and restart to apply credentials
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved. Device will restart and attempt to connect.\"}");

    Serial.println("WiFi credentials saved. Restarting...");
    delay(2000);
    ESP.restart();
}

void handleAPIReloadScreens()
{
    // PHASE 2 FINAL: Reboot-based workflow (avoids mutex/context issues)
    Serial.println("[API] Layout reload requested - rebooting device");

    server.send(200, "application/json",
                "{\"status\":\"Rebooting device to load new layouts...\",\"message\":\"Device will restart in 1 second\"}");

    delay(1000); // Let response send
    ESP.restart();
}

void handleAPIRTCSet()
{
    // Parse date and time from POST parameters
    if (!server.hasArg("date") || !server.hasArg("time"))
    {
        server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"Missing date or time parameter\"}");
        return;
    }

    String dateStr = server.arg("date"); // Format: YYYY-MM-DD
    String timeStr = server.arg("time"); // Format: HH:MM:SS

    // Parse date: YYYY-MM-DD
    int year = dateStr.substring(0, 4).toInt();
    int month = dateStr.substring(5, 7).toInt();
    int day = dateStr.substring(8, 10).toInt();

    // Parse time: HH:MM:SS
    int hour = timeStr.substring(0, 2).toInt();
    int minute = timeStr.substring(3, 5).toInt();
    int second = timeStr.substring(6, 8).toInt();

    // Validate ranges
    if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > 31 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59)
    {
        server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"Invalid date/time values\"}");
        return;
    }

    // Set the RTC time
    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    Serial.printf("[RTC] Time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);

    server.send(200, "application/json",
                "{\"success\":true,\"message\":\"RTC time updated successfully\"}");
}

void handleSaveJSON()
{
    if (!server.hasArg("filename") || !server.hasArg("content"))
    {
        server.send(400, "text/plain", "Missing filename or content");
        return;
    }

    String filename = server.arg("filename");
    String content = server.arg("content");

    File file = SD.open(filename, FILE_WRITE);
    yield();
    if (!file)
    {
        server.send(500, "text/plain", "Failed to open file for writing");
        return;
    }

    file.print(content);
    yield();
    file.close();
    yield();

    server.send(200, "text/plain", "File saved successfully");
}

// ========== Upload Handlers ==========

// Static variables shared between upload handler and completion handler
static String uploadData;
static String uploadFilename;
static bool uploadError = false;

void handleUploadJSON()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadError = false;
        uploadData = "";
        uploadFilename = upload.filename;

        // Validate filename
        if (!uploadFilename.endsWith(".json"))
        {
            Serial.println("[Upload] Not a JSON file");
            uploadError = true;
            return;
        }

        Serial.printf("[Upload] Starting: %s\n", uploadFilename.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!uploadError && upload.currentSize)
        {
            // Accumulate data in memory
            if (uploadData.length() + upload.currentSize > 8192)
            {
                Serial.println("[Upload] File too large (max 8KB)");
                uploadError = true;
                return;
            }
            // Append chunk to uploadData
            for (size_t i = 0; i < upload.currentSize; i++)
            {
                uploadData += (char)upload.buf[i];
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (!uploadError)
        {
            // PHASE 2: Write directly to SPIFFS (safe - no SD mutex issues)
            String filepath = "/screens/" + uploadFilename;

            Serial.printf("[Upload] Saving %d bytes to SPIFFS: %s\n",
                          uploadData.length(), filepath.c_str());

            if (storage.saveFile(filepath.c_str(), uploadData))
            {
                Serial.println("[Upload] SUCCESS: Saved to SPIFFS");
                uploadError = false;
            }
            else
            {
                Serial.println("[Upload] ERROR: SPIFFS write failed");
                uploadError = true;
            }
        }
        // Clear accumulated data
        uploadData = "";
        uploadFilename = "";
    }
}

void handleUploadComplete()
{
    // This is called after the upload finishes - send response to client
    if (uploadError)
    {
        server.send(500, "application/json", "{\"success\":false,\"message\":\"Upload failed\"}");
    }
    else
    {
        server.send(200, "application/json",
                    "{\"success\":true,\"message\":\"Uploaded to SPIFFS successfully\"}");
    }
}
