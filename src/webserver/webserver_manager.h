#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>

class WebServerManager {
private:
    WebServer* server;
    static const int SERVER_PORT = 80;
    void listDirRecursiveStreaming(File dir, String prefix, bool& first, int depth);

public:
    WebServerManager();
    ~WebServerManager();
    void begin();
    void handleClient();
    void stop();
    void setupScreenRoutes();
    void setupFileRoutes();
    void setupSchemaRoutes();
    void setupLegacyRoutes();
    bool isConnected() const;
};

extern WebServerManager webServer;

#endif
