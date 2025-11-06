#include "web_server.h"
#include "web_handlers.h"
#include <WebServer.h>

// External reference
extern WebServer server;

void setupWebServer()
{
    // ========== Page Routes ==========
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/admin", HTTP_GET, handleAdmin);
    server.on("/wifi", HTTP_GET, handleWiFi);
    server.on("/upload", HTTP_GET, handleUpload);
    server.on("/editor", HTTP_GET, handleEditor);

    // ========== API Routes (GET) ==========
    server.on("/api/config", HTTP_GET, handleAPIConfig);
    server.on("/api/status", HTTP_GET, handleAPIStatus);
    server.on("/api/rtc", HTTP_GET, handleAPIRTC);
    server.on("/api/upload-status", HTTP_GET, handleUploadStatus);
    server.on("/get-json", HTTP_GET, handleGetJSON);

    // ========== API Routes (POST) ==========
    server.on("/api/save", HTTP_POST, handleAPISave);
    server.on("/api/admin/save", HTTP_POST, handleAPIAdminSave);
    server.on("/api/reset-wifi", HTTP_POST, handleAPIResetWiFi);
    server.on("/api/restart", HTTP_POST, handleAPIRestart);
    server.on("/api/wifi/connect", HTTP_POST, handleAPIWiFiConnect);
    server.on("/api/reload-screens", HTTP_POST, handleAPIReloadScreens);
    server.on("/api/reload-screens", HTTP_GET, handleAPIReloadScreens); // Also accept GET
    server.on("/api/rtc/set", HTTP_POST, handleAPIRTCSet);
    server.on("/save-json", HTTP_POST, handleSaveJSON);

    // ========== Special Routes ==========
    // Reboot endpoint (GET)
    server.on("/api/reboot", HTTP_GET, handleAPIReboot);

    // Upload endpoint (multipart form data)
    server.on("/upload-json", HTTP_POST, handleUploadComplete, handleUploadJSON);

    // Start the server
    server.begin();
    Serial.println("Web server started");
}
