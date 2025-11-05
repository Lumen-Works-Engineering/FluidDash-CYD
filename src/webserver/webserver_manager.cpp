#include "webserver_manager.h"
#include "sd_mutex.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <vector>

// Global instance
WebServerManager webServer;

// Constructor
WebServerManager::WebServerManager() {
    server = new AsyncWebServer(80);
}

// Destructor
WebServerManager::~WebServerManager() {
    stop();
    delete server;
}

// Start the web server
void WebServerManager::begin() {
    setupScreenRoutes();
    setupSchemaRoutes();
    setupFileRoutes();
    setupLegacyRoutes();

    // Start server
    server->begin();
    Serial.println("AsyncWebServer started");
}

// Stop the web server
void WebServerManager::stop() {
    server->end();
    Serial.println("AsyncWebServer stopped");
}

// Helper function to list directory recursively
void WebServerManager::listDirRecursive(File dir, String prefix, JsonArray& files, int depth) {
    if (depth > 3) return;  // Limit recursion depth

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }

        String fullPath = prefix + "/" + String(entry.name());

        if (entry.isDirectory()) {
            listDirRecursive(entry, fullPath, files, depth + 1);
        } else {
            JsonObject fileObj = files.createNestedObject();
            fileObj["name"] = fullPath;
            fileObj["size"] = entry.size();
        }
        entry.close();
    }
}

// Setup screen-related routes
void WebServerManager::setupScreenRoutes() {
    // GET /api/screens - List all screen JSON files
    server->on("/api/screens", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!SD_MUTEX_LOCK()) {
            Serial.println("[API] Failed to lock SD mutex for /api/screens");
            request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
            return;
        }

        File screensDir = SD.open("/screens");
        if (!screensDir || !screensDir.isDirectory()) {
            SD_MUTEX_UNLOCK();
            request->send(500, "application/json", "{\"error\":\"Failed to open screens directory\"}");
            return;
        }

        JsonDocument doc;
        JsonArray screens = doc.createNestedArray("screens");

        while (true) {
            File entry = screensDir.openNextFile();
            if (!entry) {
                break;
            }

            if (!entry.isDirectory()) {
                String filename = String(entry.name());
                if (filename.endsWith(".json")) {
                    JsonObject screenObj = screens.createNestedObject();
                    screenObj["name"] = filename;
                    screenObj["size"] = entry.size();
                }
            }
            entry.close();
        }
        screensDir.close();
        SD_MUTEX_UNLOCK();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/upload-screen - Upload a screen JSON file
    server->on("/api/upload-screen", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"success\":true}");
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            static File uploadFile;
            static bool mutexLocked = false;

            if (index == 0) {
                Serial.printf("Upload start: %s\n", filename.c_str());

                if (!SD_MUTEX_LOCK()) {
                    Serial.println("[API] Failed to lock SD mutex for /api/upload-screen");
                    mutexLocked = false;
                    return;
                }
                mutexLocked = true;

                if (!SD.exists("/screens")) {
                    SD.mkdir("/screens");
                }

                String filepath = "/screens/" + filename;
                uploadFile = SD.open(filepath, FILE_WRITE);

                if (!uploadFile) {
                    SD_MUTEX_UNLOCK();
                    mutexLocked = false;
                    Serial.println("Failed to open file for writing");
                    return;
                }
            }

            if (uploadFile && mutexLocked) {
                uploadFile.write(data, len);
            }

            if (final) {
                if (uploadFile) {
                    uploadFile.close();
                }
                if (mutexLocked) {
                    SD_MUTEX_UNLOCK();
                    mutexLocked = false;
                }
                Serial.printf("Upload complete: %s, total size: %d\n", filename.c_str(), index + len);
            }
        }
    );

    // DELETE /api/delete-screen?filename=xxx
    server->on("/api/delete-screen", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("filename")) {
            request->send(400, "application/json", "{\"error\":\"Missing filename parameter\"}");
            return;
        }

        String filename = request->getParam("filename")->value();
        String filepath = "/screens/" + filename;

        if (!SD_MUTEX_LOCK()) {
            Serial.println("[API] Failed to lock SD mutex for /api/delete-screen");
            request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
            return;
        }

        if (!SD.exists(filepath)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "application/json", "{\"error\":\"File not found\"}");
            return;
        }

        bool success = SD.remove(filepath);
        SD_MUTEX_UNLOCK();

        if (success) {
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to delete file\"}");
        }
    });
}

// Setup schema-related routes
void WebServerManager::setupSchemaRoutes() {
    // GET /api/schema/screen-elements - Return JSON schema for screen elements
    server->on("/api/schema/screen-elements", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;

        doc["title"] = "Screen Element Schema";
        doc["type"] = "object";

        JsonObject properties = doc.createNestedObject("properties");

        // Define element type
        JsonObject elementType = properties.createNestedObject("type");
        elementType["type"] = "string";
        elementType["enum"][0] = "text";
        elementType["enum"][1] = "gauge";
        elementType["enum"][2] = "bar";
        elementType["enum"][3] = "icon";

        // Define position
        JsonObject x = properties.createNestedObject("x");
        x["type"] = "integer";
        JsonObject y = properties.createNestedObject("y");
        y["type"] = "integer";

        // Define dimensions
        JsonObject width = properties.createNestedObject("width");
        width["type"] = "integer";
        JsonObject height = properties.createNestedObject("height");
        height["type"] = "integer";

        // Define data source
        JsonObject dataSource = properties.createNestedObject("dataSource");
        dataSource["type"] = "string";

        // Define color
        JsonObject color = properties.createNestedObject("color");
        color["type"] = "string";

        // Required fields
        doc["required"][0] = "type";
        doc["required"][1] = "x";
        doc["required"][2] = "y";

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

// Setup file management routes
void WebServerManager::setupFileRoutes() {
    // GET /api/files - List all files on SD card
    server->on("/api/files", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!SD_MUTEX_LOCK()) {
            Serial.println("[API] Failed to lock SD mutex for /api/files");
            request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
            return;
        }

        File root = SD.open("/");
        if (!root || !root.isDirectory()) {
            SD_MUTEX_UNLOCK();
            request->send(500, "application/json", "{\"error\":\"Failed to open root directory\"}");
            return;
        }

        JsonDocument doc;
        JsonArray files = doc.createNestedArray("files");

        listDirRecursive(root, "", files, 0);

        root.close();
        SD_MUTEX_UNLOCK();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // GET /api/download?path=xxx - Download a file
    server->on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("path")) {
            request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
            return;
        }

        String filepath = request->getParam("path")->value();

        if (!SD_MUTEX_LOCK()) {
            Serial.println("[API] Failed to lock SD mutex for /api/download");
            request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
            return;
        }

        if (!SD.exists(filepath)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "application/json", "{\"error\":\"File not found\"}");
            return;
        }

        File file = SD.open(filepath, FILE_READ);
        if (!file) {
            SD_MUTEX_UNLOCK();
            request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
            return;
        }

        // Read entire file into memory before unlocking mutex
        size_t fileSize = file.size();
        if (fileSize > 102400) {  // 100KB limit
            file.close();
            SD_MUTEX_UNLOCK();
            request->send(413, "application/json", "{\"error\":\"File too large\"}");
            return;
        }

        String content = file.readString();
        file.close();
        SD_MUTEX_UNLOCK();

        // Now safe to send - file is closed, mutex is unlocked
        request->send(200, "application/octet-stream", content);
    });

    // DELETE /api/delete-file?path=xxx - Delete a file
    server->on("/api/delete-file", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("path")) {
            request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
            return;
        }

        String filepath = request->getParam("path")->value();

        if (!SD_MUTEX_LOCK()) {
            Serial.println("[API] Failed to lock SD mutex for /api/delete-file");
            request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
            return;
        }

        if (!SD.exists(filepath)) {
            SD_MUTEX_UNLOCK();
            request->send(404, "application/json", "{\"error\":\"File not found\"}");
            return;
        }

        bool success = SD.remove(filepath);
        SD_MUTEX_UNLOCK();

        if (success) {
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to delete file\"}");
        }
    });

    // GET /api/disk-usage - Get SD card disk usage
    server->on("/api/disk-usage", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!SD_MUTEX_LOCK()) {
            Serial.println("[API] Failed to lock SD mutex for /api/disk-usage");
            request->send(500, "application/json", "{\"error\":\"SD card busy or mutex error\"}");
            return;
        }

        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);
        uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);

        SD_MUTEX_UNLOCK();

        JsonDocument doc;
        doc["cardSizeMB"] = cardSize;
        doc["totalMB"] = totalBytes;
        doc["usedMB"] = usedBytes;
        doc["freeMB"] = totalBytes - usedBytes;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

// Setup legacy routes for backward compatibility
void WebServerManager::setupLegacyRoutes() {
    // GET / - Root page with API documentation
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<html><head><title>FluidDash API</title></head><body>";
        html += "<h1>FluidDash Web Server</h1>";
        html += "<p>AsyncWebServer is running. Available endpoints:</p>";
        html += "<h2>Screen Management</h2>";
        html += "<ul>";
        html += "<li>GET <a href='/api/screens'>/api/screens</a> - List screen JSON files</li>";
        html += "<li>POST /api/upload-screen - Upload screen JSON file</li>";
        html += "<li>DELETE /api/delete-screen?filename=xxx - Delete screen file</li>";
        html += "</ul>";
        html += "<h2>File Management</h2>";
        html += "<ul>";
        html += "<li>GET <a href='/api/files'>/api/files</a> - List all SD card files</li>";
        html += "<li>GET /api/download?path=xxx - Download a file</li>";
        html += "<li>DELETE /api/delete-file?path=xxx - Delete a file</li>";
        html += "<li>GET <a href='/api/disk-usage'>/api/disk-usage</a> - Get SD card usage</li>";
        html += "</ul>";
        html += "<h2>System</h2>";
        html += "<ul>";
        html += "<li>GET <a href='/api/status'>/api/status</a> - System status</li>";
        html += "<li>GET <a href='/api/config'>/api/config</a> - Current configuration</li>";
        html += "<li>GET <a href='/api/sensor-mappings'>/api/sensor-mappings</a> - Sensor mappings</li>";
        html += "</ul>";
        html += "<h2>Schema</h2>";
        html += "<ul>";
        html += "<li>GET <a href='/api/schema/screen-elements'>/api/schema/screen-elements</a> - Screen element schema</li>";
        html += "</ul>";
        html += "</body></html>";
        request->send(200, "text/html", html);
    });

    // GET /settings - Settings page
    server->on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<html><body><h1>Settings</h1><p>Settings page placeholder</p></body></html>");
    });

    // GET /admin - Admin page
    server->on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<html><body><h1>Admin</h1><p>Admin page placeholder</p></body></html>");
    });

    // GET /wifi - WiFi configuration page
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<html><body><h1>WiFi Configuration</h1><p>WiFi config placeholder</p></body></html>");
    });

    // GET /api/config - Get current configuration
    server->on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["wifi"]["ssid"] = "FluidDash";
        doc["wifi"]["connected"] = true;
        doc["display"]["brightness"] = 255;
        doc["display"]["timeout"] = 0;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // GET /api/status - Get system status
    server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["chipModel"] = ESP.getChipModel();
        doc["sdCardPresent"] = SD.cardSize() > 0;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/save - Save configuration
    server->on("/api/save", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"success\":true}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Handle body data
            if (index == 0) {
                Serial.printf("Receiving config data, total: %d\n", total);
            }

            if (index + len == total) {
                Serial.println("Config data received completely");
                // Parse and save configuration here
            }
        }
    );

    // GET /api/sensor-mappings - Get sensor mappings
    server->on("/api/sensor-mappings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray mappings = doc.createNestedArray("mappings");

        JsonObject mapping1 = mappings.createNestedObject();
        mapping1["id"] = "temp1";
        mapping1["name"] = "Engine Temperature";
        mapping1["unit"] = "C";
        mapping1["min"] = 0;
        mapping1["max"] = 150;

        JsonObject mapping2 = mappings.createNestedObject();
        mapping2["id"] = "pressure1";
        mapping2["name"] = "Oil Pressure";
        mapping2["unit"] = "PSI";
        mapping2["min"] = 0;
        mapping2["max"] = 100;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/sensor-mappings - Update sensor mappings
    server->on("/api/sensor-mappings", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"success\":true}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index + len == total) {
                Serial.println("Sensor mappings updated");
                // Parse and save sensor mappings here
            }
        }
    );
}

// Check if web server is connected/running
bool WebServerManager::isConnected() const {
    return (server != nullptr);
}
