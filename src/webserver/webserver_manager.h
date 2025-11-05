#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>

class WebServerManager {
private:
    AsyncWebServer* server;
    static const int SERVER_PORT = 80;
    void listDirRecursive(File dir, String prefix, JsonArray& files, int depth);

public:
    WebServerManager();
    ~WebServerManager();
    void begin();
    void stop();
    void setupScreenRoutes();
    void setupFileRoutes();
    void setupSchemaRoutes();
    void setupLegacyRoutes();
    bool isConnected() const;
};

extern WebServerManager webServer;

#endif
