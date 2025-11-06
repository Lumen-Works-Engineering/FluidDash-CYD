#include "html_pages.h"
#include "config/config.h"
#include "state/system_state.h"
#include <WiFi.h>

// External references to PROGMEM HTML templates (defined in main.cpp)
extern const char MAIN_HTML[] PROGMEM;
extern const char SETTINGS_HTML[] PROGMEM;
extern const char ADMIN_HTML[] PROGMEM;
extern const char WIFI_CONFIG_HTML[] PROGMEM;

String getMainHTML()
{
    String html = String(FPSTR(MAIN_HTML));

    // Replace all placeholders with dynamic content
    html.replace("%DEVICE_NAME%", cfg.device_name);
    html.replace("%IP_ADDRESS%", WiFi.localIP().toString());
    html.replace("%FLUIDNC_IP%", cfg.fluidnc_ip);

    return html;
}

String getSettingsHTML()
{
    String html = String(FPSTR(SETTINGS_HTML));

    // Replace numeric input values
    html.replace("%TEMP_LOW%", String(cfg.temp_threshold_low));
    html.replace("%TEMP_HIGH%", String(cfg.temp_threshold_high));
    html.replace("%FAN_MIN%", String(cfg.fan_min_speed));
    html.replace("%PSU_LOW%", String(cfg.psu_alert_low));
    html.replace("%PSU_HIGH%", String(cfg.psu_alert_high));

    // Replace graph timespan selected options
    html.replace("%GRAPH_TIME_60%", cfg.graph_timespan_seconds == 60 ? "selected" : "");
    html.replace("%GRAPH_TIME_300%", cfg.graph_timespan_seconds == 300 ? "selected" : "");
    html.replace("%GRAPH_TIME_600%", cfg.graph_timespan_seconds == 600 ? "selected" : "");
    html.replace("%GRAPH_TIME_1800%", cfg.graph_timespan_seconds == 1800 ? "selected" : "");
    html.replace("%GRAPH_TIME_3600%", cfg.graph_timespan_seconds == 3600 ? "selected" : "");

    // Replace graph interval selected options
    html.replace("%GRAPH_INT_1%", cfg.graph_update_interval == 1 ? "selected" : "");
    html.replace("%GRAPH_INT_5%", cfg.graph_update_interval == 5 ? "selected" : "");
    html.replace("%GRAPH_INT_10%", cfg.graph_update_interval == 10 ? "selected" : "");
    html.replace("%GRAPH_INT_30%", cfg.graph_update_interval == 30 ? "selected" : "");
    html.replace("%GRAPH_INT_60%", cfg.graph_update_interval == 60 ? "selected" : "");

    // Replace coordinate decimal places selected options
    html.replace("%COORD_DEC_2%", cfg.coord_decimal_places == 2 ? "selected" : "");
    html.replace("%COORD_DEC_3%", cfg.coord_decimal_places == 3 ? "selected" : "");

    return html;
}

String getAdminHTML()
{
    String html = String(FPSTR(ADMIN_HTML));

    // Replace calibration offset values (with 2 decimal places for temp)
    html.replace("%CAL_X%", String(cfg.temp_offset_x, 2));
    html.replace("%CAL_YL%", String(cfg.temp_offset_yl, 2));
    html.replace("%CAL_YR%", String(cfg.temp_offset_yr, 2));
    html.replace("%CAL_Z%", String(cfg.temp_offset_z, 2));

    // Replace PSU calibration value (with 3 decimal places)
    html.replace("%PSU_CAL%", String(cfg.psu_voltage_cal, 3));

    return html;
}

String getWiFiConfigHTML()
{
    String html = String(FPSTR(WIFI_CONFIG_HTML));

    // Get current WiFi status
    String currentSSID = WiFi.SSID();
    String currentIP = WiFi.localIP().toString();
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    bool isAPMode = systemState.inAPMode;

    // Build WiFi status section
    String wifiStatus = "<div class='status ";
    if (isAPMode)
    {
        wifiStatus += "status-ap'>🔧 AP Mode Active - Configure WiFi to connect to your network</div>";
    }
    else if (isConnected)
    {
        wifiStatus += "status-connected'>✅ Connected to: " + currentSSID + "<br>IP: " + currentIP + "</div>";
    }
    else
    {
        wifiStatus += "status-disconnected'>❌ Not Connected - Configure WiFi below</div>";
    }

    // Replace placeholders
    html.replace("%WIFI_STATUS%", wifiStatus);
    html.replace("%CURRENT_SSID%", currentSSID);

    return html;
}
