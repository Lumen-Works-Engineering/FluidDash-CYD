#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <ESPmDNS.h>

// ========== WiFi Management ==========
void setupWiFiManager();

// ========== FluidNC WebSocket Client ==========
void connectFluidNC();
void discoverFluidNC();
void fluidNCWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void parseFluidNCStatus(String status);

// ========== External Variables ==========
// These are defined in main.cpp and accessed by network functions
extern WebSocketsClient webSocket;
extern WiFiManager wm;

// FluidNC status variables
extern String machineState;
extern float posX, posY, posZ, posA;
extern float wposX, wposY, wposZ, wposA;
extern int feedRate;
extern int spindleRPM;
extern bool fluidncConnected;
extern unsigned long jobStartTime;
extern bool isJobRunning;

// Extended status fields
extern int feedOverride;
extern int rapidOverride;
extern int spindleOverride;
extern float wcoX, wcoY, wcoZ, wcoA;

// WebSocket reporting
extern bool autoReportingEnabled;
extern unsigned long reportingSetupTime;

// Debug control
extern bool debugWebSocket;

#endif // NETWORK_H
