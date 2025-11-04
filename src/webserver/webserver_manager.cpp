#include "webserver_manager.h"
#include "../config/config.h"
#include "../sensors/sensors.h"
#include "../display/screen_renderer.h"
#include "../utils/utils.h"
#include <Preferences.h>
#include <WiFiManager.h>

// External references from main.cpp
extern bool sdCardAvailable;
extern Preferences prefs;
extern WiFiManager wm;
extern DisplayMode currentMode;

// External functions from main.cpp
extern String getMainHTML();
extern String getSettingsHTML();
extern String getAdminHTML();
extern String getWiFiConfigHTML();
extern String getConfigJSON();
extern String getStatusJSON();

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
    setupLegacyRoutes();    // Existing web interface
    setupScreenRoutes();    // New screen management
    setupFileRoutes();      // File management
    setupSchemaRoutes();    // Element discovery

    // Serve static files from SD card (if available)
    if (sdCardAvailable) {
        // Note: WebServer doesn't have serveStatic like AsyncWebServer
        // We'll handle static file serving manually in setupFileRoutes
        log_i("SD card available - static files will be served on demand");
    }

    // Root redirect to main interface
    server->on("/", HTTP_GET, [this]() {
        yield(); // Feed watchdog before heavy operation
        String html = getMainHTML();
        yield(); // Feed watchdog after heavy operation
        server->send(200, "text/html", html);
    });

    // 404 handler - simplified to avoid SD card mutex issues
    // Static file serving from SD removed due to FreeRTOS semaphore conflicts
    // Use explicit API routes instead (/api/download?path=...)
    server->onNotFound([this]() {
        String path = server->uri();
        log_w("404 Not Found: %s", path.c_str());
        server->send(404, "text/plain", "Not Found: " + path);
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

// ========== SCREEN MANAGEMENT ROUTES ==========

void WebServerManager::setupScreenRoutes() {
    // Get all screen files - STREAMING version to prevent heap corruption
    server->on("/api/screens", HTTP_GET, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "application/json", "{\"error\":\"SD card not available\"}");
            return;
        }

        File dir = SD.open("/screens");
        if (!dir) {
            server->send(404, "application/json", "{\"files\":[]}");
            return;
        }

        // Stream response instead of building large JSON in memory
        server->setContentLength(CONTENT_LENGTH_UNKNOWN);
        server->send(200, "application/json", "");

        // Send opening
        server->sendContent("{\"files\":[");

        bool first = true;
        int fileCount = 0;
        File f = dir.openNextFile();
        while (f) {
            yield(); // Prevent watchdog

            if (!f.isDirectory() && String(f.name()).endsWith(".json")) {
                if (!first) {
                    server->sendContent(",");
                }

                // Build small JSON object one at a time using fixed-size buffer
                char buffer[256];
                snprintf(buffer, sizeof(buffer),
                    "{\"name\":\"%s\",\"size\":%d,\"modified\":%ld}",
                    f.name(), (int)f.size(), (long)f.getLastWrite());
                server->sendContent(buffer);

                first = false;
                fileCount++;
            }

            f.close(); // Close immediately after reading
            f = dir.openNextFile();

            // Safety limit
            if (fileCount > 100) {
                log_w("Too many files, breaking");
                break;
            }
        }

        server->sendContent("]}");
        dir.close();
    });

    // Download specific screen file
    server->on("/api/screens/*", HTTP_GET, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "text/plain", "SD card not available");
            return;
        }

        String uri = server->uri();
        String filename = uri.substring(uri.lastIndexOf("/") + 1);
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

        server->streamFile(file, "application/json");
        file.close();
    });

    // Upload/Save screen file - FROM EDITOR
    server->on("/api/upload-screen", HTTP_POST, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "application/json", "{\"error\":\"SD card not available\"}");
            return;
        }

        String body = server->arg("plain");

        // Parse JSON body
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        // Extract filename from data
        String filename = doc["filename"] | "new_screen.json";
        JsonObject screenData = doc["data"].as<JsonObject>();

        String path = "/screens/" + filename;

        // Validate it's valid screen JSON
        if (!screenData["elements"].is<JsonArray>()) {
            server->send(400, "application/json", "{\"error\":\"Invalid screen format\"}");
            return;
        }

        // Write file
        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            server->send(500, "application/json", "{\"error\":\"Cannot create file\"}");
            return;
        }

        // Serialize just the screen data (not the filename wrapper)
        serializeJson(screenData, file);
        file.close();

        // Reload screens based on filename
        if (filename == "monitor.json") {
            loadScreenConfig(path.c_str(), monitorLayout);
        } else if (filename == "alignment.json") {
            loadScreenConfig(path.c_str(), alignmentLayout);
        } else if (filename == "graph.json") {
            loadScreenConfig(path.c_str(), graphLayout);
        } else if (filename == "network.json") {
            loadScreenConfig(path.c_str(), networkLayout);
        }

        String responseStr = "{\"status\":\"saved\",\"file\":\"" + filename + "\"}";
        server->send(200, "application/json", responseStr);

        log_i("Screen saved: %s", path.c_str());
    });

    // Delete screen file
    server->on("/api/delete-screen", HTTP_POST, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "text/plain", "SD card not available");
            return;
        }

        String filename = server->arg("filename");
        String path = "/screens/" + filename;

        if (!SD.exists(path)) {
            server->send(404, "text/plain", "File not found");
            return;
        }

        if (SD.remove(path)) {
            server->send(200, "text/plain", "File deleted");
        } else {
            server->send(500, "text/plain", "Delete failed");
        }
    });

    // Reload screens endpoint
    server->on("/api/reload-screens", HTTP_GET, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "application/json", "{\"success\":false,\"message\":\"SD card not available\"}");
            return;
        }

        log_i("Reloading screen layouts...");

        int loaded = 0;
        if (loadScreenConfig("/screens/monitor.json", monitorLayout)) loaded++;
        if (loadScreenConfig("/screens/alignment.json", alignmentLayout)) loaded++;
        if (loadScreenConfig("/screens/graph.json", graphLayout)) loaded++;
        if (loadScreenConfig("/screens/network.json", networkLayout)) loaded++;

        char response[128];
        snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"Reloaded %d layouts\"}", loaded);
        server->send(200, "application/json", response);

        log_i("Reloaded %d layouts", loaded);
    });
}

// ========== SCHEMA/ELEMENT DISCOVERY ROUTES ==========

void WebServerManager::setupSchemaRoutes() {
    // Return available screen elements schema for editor
    server->on("/api/schema/screen-elements", HTTP_GET, [this]() {
        JsonDocument doc;

        // Coordinate data sources
        JsonArray coords = doc["coordinates"].to<JsonArray>();
        coords.add("wposX");
        coords.add("wposY");
        coords.add("wposZ");
        coords.add("wposA");
        coords.add("posX");
        coords.add("posY");
        coords.add("posZ");
        coords.add("posA");

        // Temperature sensors (discover actual connected sensors)
        JsonArray temps = doc["temperatures"].to<JsonArray>();
        // Add discovered DS18B20 sensors
        for (int i = 0; i < 4; i++) {
            // Check if temperature is non-zero (sensor connected)
            if (temperatures[i] != 0 || i == 0) {  // Always include temp0
                String tempName = "temp" + String(i);
                temps.add(tempName);
            }
        }

        // Status fields
        JsonArray status = doc["status"].to<JsonArray>();
        status.add("machineState");
        status.add("feedRate");
        status.add("spindleRPM");

        // System fields
        JsonArray system = doc["system"].to<JsonArray>();
        system.add("psuVoltage");
        system.add("fanSpeed");
        system.add("ipAddress");
        system.add("ssid");
        system.add("deviceName");
        system.add("fluidncIP");

        // Element types available
        JsonArray types = doc["elementTypes"].to<JsonArray>();
        types.add("rect");
        types.add("line");
        types.add("text");
        types.add("dynamic");
        types.add("temp");
        types.add("status");
        types.add("progress");
        types.add("graph");

        // Colors reference (RGB565 hex)
        JsonObject colors = doc["colors"].to<JsonObject>();
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

// ========== FILE MANAGEMENT ROUTES ==========

void WebServerManager::setupFileRoutes() {
    // List all files on SD card - STREAMING version to prevent heap corruption
    server->on("/api/files", HTTP_GET, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "application/json", "{\"error\":\"SD card not available\"}");
            return;
        }

        File root = SD.open("/");
        if (!root) {
            server->send(500, "application/json", "{\"files\":[]}");
            return;
        }

        // Stream response instead of building large JSON in memory
        server->setContentLength(CONTENT_LENGTH_UNKNOWN);
        server->send(200, "application/json", "");

        server->sendContent("{\"files\":[");

        bool first = true;
        listDirRecursiveStreaming(root, "", first, 0);

        server->sendContent("]}");
        root.close();
    });

    // Download file (generic)
    server->on("/api/download", HTTP_GET, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "text/plain", "SD card not available");
            return;
        }

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

        // Determine content type
        String contentType = "application/octet-stream";
        if (filename.endsWith(".json")) contentType = "application/json";
        else if (filename.endsWith(".txt")) contentType = "text/plain";
        else if (filename.endsWith(".html")) contentType = "text/html";

        server->sendHeader("Content-Disposition", "attachment; filename=" + filename);
        server->streamFile(file, contentType);
        file.close();
    });

    // Delete file
    server->on("/api/delete-file", HTTP_POST, [this]() {
        if (!sdCardAvailable) {
            server->send(503, "text/plain", "SD card not available");
            return;
        }

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
        JsonDocument doc;

        if (sdCardAvailable) {
            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            uint64_t usedSize = SD.usedBytes() / (1024 * 1024);

            doc["total"] = (long)(cardSize * 1024 * 1024);
            doc["used"] = (long)(usedSize * 1024 * 1024);
            doc["free"] = (long)((cardSize - usedSize) * 1024 * 1024);
        } else {
            doc["total"] = 0;
            doc["used"] = 0;
            doc["free"] = 0;
        }

        String response;
        serializeJson(doc, response);
        server->send(200, "application/json", response);
    });
}

// Helper function for streaming recursive directory listing
void WebServerManager::listDirRecursiveStreaming(File dir, String prefix, bool& first, int depth) {
    if (depth > 3) return;  // Limit recursion depth

    int fileCount = 0;
    File file = dir.openNextFile();
    while (file) {
        yield(); // Feed watchdog during iteration
        String name = String(file.name());
        String fullPath = prefix + "/" + name;

        if (file.isDirectory()) {
            // Recursively list subdirectories
            listDirRecursiveStreaming(file, fullPath, first, depth + 1);
        } else {
            if (!first) {
                server->sendContent(",");
            }

            // Build small JSON object using fixed-size buffer
            char buffer[384];
            snprintf(buffer, sizeof(buffer),
                "{\"path\":\"%s\",\"name\":\"%s\",\"size\":%d,\"modified\":%ld}",
                fullPath.c_str(), name.c_str(), (int)file.size(), (long)file.getLastWrite());
            server->sendContent(buffer);

            first = false;
            fileCount++;
        }

        file.close();
        file = dir.openNextFile();

        // Safety: prevent processing too many files
        if (fileCount > 200) {
            log_w("Too many files at depth %d, breaking", depth);
            break;
        }
    }
}

// ========== LEGACY WEB INTERFACE ROUTES ==========

void WebServerManager::setupLegacyRoutes() {
    // User settings page
    server->on("/settings", HTTP_GET, [](){
        yield();
        String html = getSettingsHTML();
        yield();
        webServer.server->send(200, "text/html", html);
    });

    // Admin/calibration page
    server->on("/admin", HTTP_GET, [](){
        yield();
        String html = getAdminHTML();
        yield();
        webServer.server->send(200, "text/html", html);
    });

    // WiFi configuration page
    server->on("/wifi", HTTP_GET, [](){
        yield();
        String html = getWiFiConfigHTML();
        yield();
        webServer.server->send(200, "text/html", html);
    });

    // API: Get current config as JSON
    server->on("/api/config", HTTP_GET, [](){
        yield();
        String json = getConfigJSON();
        yield();
        webServer.server->send(200, "application/json", json);
    });

    // API: Get current status as JSON
    server->on("/api/status", HTTP_GET, [](){
        yield();
        String json = getStatusJSON();
        yield();
        webServer.server->send(200, "application/json", json);
    });

    // API: Save settings
    server->on("/api/save", HTTP_POST, [this]() {
        // Update config from POST parameters
        if (server->hasArg("temp_low")) {
            cfg.temp_threshold_low = server->arg("temp_low").toFloat();
        }
        if (server->hasArg("temp_high")) {
            cfg.temp_threshold_high = server->arg("temp_high").toFloat();
        }
        if (server->hasArg("fan_min")) {
            cfg.fan_min_speed = server->arg("fan_min").toInt();
        }
        if (server->hasArg("graph_time")) {
            uint16_t newTime = server->arg("graph_time").toInt();
            if (newTime != cfg.graph_timespan_seconds) {
                cfg.graph_timespan_seconds = newTime;
                allocateHistoryBuffer(); // Reallocate with new size
            }
        }
        if (server->hasArg("graph_interval")) {
            cfg.graph_update_interval = server->arg("graph_interval").toInt();
        }
        if (server->hasArg("psu_low")) {
            cfg.psu_alert_low = server->arg("psu_low").toFloat();
        }
        if (server->hasArg("psu_high")) {
            cfg.psu_alert_high = server->arg("psu_high").toFloat();
        }
        if (server->hasArg("coord_decimals")) {
            cfg.coord_decimal_places = server->arg("coord_decimals").toInt();
        }

        saveConfig();
        server->send(200, "text/plain", "Settings saved successfully");
    });

    // API: Save admin/calibration settings
    server->on("/api/admin/save", HTTP_POST, [this]() {
        if (server->hasArg("cal_x")) {
            cfg.temp_offset_x = server->arg("cal_x").toFloat();
        }
        if (server->hasArg("cal_yl")) {
            cfg.temp_offset_yl = server->arg("cal_yl").toFloat();
        }
        if (server->hasArg("cal_yr")) {
            cfg.temp_offset_yr = server->arg("cal_yr").toFloat();
        }
        if (server->hasArg("cal_z")) {
            cfg.temp_offset_z = server->arg("cal_z").toFloat();
        }
        if (server->hasArg("psu_cal")) {
            cfg.psu_voltage_cal = server->arg("psu_cal").toFloat();
        }

        saveConfig();
        server->send(200, "text/plain", "Calibration saved successfully");
    });

    // Reset WiFi settings
    server->on("/api/reset-wifi", HTTP_POST, [this]() {
        server->send(200, "text/plain", "Resetting WiFi - device will restart");
        delay(1000);
        wm.resetSettings();
        ESP.restart();
    });

    // Restart device
    server->on("/api/restart", HTTP_POST, [this]() {
        server->send(200, "text/plain", "Restarting...");
        delay(1000);
        ESP.restart();
    });

    // API: Connect to WiFi network
    server->on("/api/wifi/connect", HTTP_POST, [this]() {
        String ssid = "";
        String password = "";

        if (server->hasArg("ssid")) {
            ssid = server->arg("ssid");
        }
        if (server->hasArg("password")) {
            password = server->arg("password");
        }

        if (ssid.length() == 0) {
            server->send(200, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
            return;
        }

        Serial.println("Attempting to connect to: " + ssid);

        // Store credentials in preferences
        prefs.begin("fluiddash", false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", password);
        prefs.end();

        // Send response and restart to apply credentials
        server->send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved. Device will restart and attempt to connect.\"}");

        Serial.println("WiFi credentials saved. Restarting...");
        delay(2000);
        ESP.restart();
    });
}
