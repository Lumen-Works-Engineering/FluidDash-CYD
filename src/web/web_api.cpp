#include "web_api.h"
#include "config/config.h"
#include "state/system_state.h"
#include "state/fluidnc_state.h"
#include "storage_manager.h"
#include <WiFi.h>
#include <RTClib.h>
#include <ArduinoJson.h>

// External references
extern StorageManager storage;
extern RTC_DS3231 rtc;

String getConfigJSON()
{
    JsonDocument doc;

    doc["device_name"] = cfg.device_name;
    doc["fluidnc_ip"] = cfg.fluidnc_ip;
    doc["temp_low"] = cfg.temp_threshold_low;
    doc["temp_high"] = cfg.temp_threshold_high;
    doc["fan_min"] = cfg.fan_min_speed;
    doc["psu_low"] = cfg.psu_alert_low;
    doc["psu_high"] = cfg.psu_alert_high;
    doc["graph_time"] = cfg.graph_timespan_seconds;
    doc["graph_interval"] = cfg.graph_update_interval;

    String response;
    serializeJson(doc, response);
    return response;
}

String getStatusJSON()
{
    JsonDocument doc;

    // Machine state
    doc["machine_state"] = fluidncState.machineState;
    doc["connected"] = fluidncState.fluidncConnected;

    // Temperatures
    JsonArray temps = doc["temperatures"].to<JsonArray>();
    for (int i = 0; i < 4; i++)
    {
        temps.add(serialized(String(systemState.temperatures[i], 2)));
    }

    // Fan control
    doc["fan_speed"] = systemState.fanSpeed;
    doc["fan_rpm"] = systemState.fanRPM;

    // PSU voltage
    doc["psu_voltage"] = serialized(String(systemState.psuVoltage, 2));

    // Work positions
    doc["wpos_x"] = serialized(String(fluidncState.wposX, 3));
    doc["wpos_y"] = serialized(String(fluidncState.wposY, 3));
    doc["wpos_z"] = serialized(String(fluidncState.wposZ, 3));

    // Machine positions
    doc["mpos_x"] = serialized(String(fluidncState.posX, 3));
    doc["mpos_y"] = serialized(String(fluidncState.posY, 3));
    doc["mpos_z"] = serialized(String(fluidncState.posZ, 3));

    String response;
    serializeJson(doc, response);
    return response;
}

String getRTCJSON()
{
    JsonDocument doc;

    DateTime now = rtc.now();

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    doc["success"] = true;
    doc["timestamp"] = timestamp;

    String response;
    serializeJson(doc, response);
    return response;
}

String getUploadStatusJSON()
{
    JsonDocument doc;

    doc["spiffsAvailable"] = storage.isSPIFFSAvailable();
    doc["sdAvailable"] = storage.isSDAvailable();
    doc["message"] = "Upload saves to SPIFFS, auto-loads on reload";

    String response;
    serializeJson(doc, response);
    return response;
}
