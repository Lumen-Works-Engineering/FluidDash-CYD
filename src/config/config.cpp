#include "config.h"
#include <Preferences.h>

// Define the global config instance
Config cfg;

// Define screen layouts
ScreenLayout monitorLayout;
ScreenLayout alignmentLayout;
ScreenLayout graphLayout;
ScreenLayout networkLayout;
bool layoutsLoaded = false;

// Preferences object - extern (defined in main.cpp)
extern Preferences prefs;

void initDefaultConfig() {
  strcpy(cfg.device_name, "fluiddash");
  strcpy(cfg.fluidnc_ip, "192.168.73.13");
  cfg.fluidnc_port = 81;  // FluidNC WebSocket default port
  cfg.fluidnc_auto_discover = true;

  cfg.temp_threshold_low = 30.0;
  cfg.temp_threshold_high = 50.0;

  cfg.temp_offset_x = 0.0;
  cfg.temp_offset_yl = 0.0;
  cfg.temp_offset_yr = 0.0;
  cfg.temp_offset_z = 0.0;
  cfg.fan_min_speed = 30;
  cfg.fan_max_speed_limit = 100;

  cfg.psu_voltage_cal = 7.3;
  cfg.psu_alert_low = 23.0;
  cfg.psu_alert_high = 25.0;

  cfg.brightness = 255;
  cfg.default_mode = MODE_MONITOR;
  cfg.show_machine_coords = true;
  cfg.show_temp_graph = true;
  cfg.coord_decimal_places = 2;

  cfg.graph_timespan_seconds = 300;  // 5 minutes default
  cfg.graph_update_interval = 5;      // 5 seconds per point

  cfg.use_fahrenheit = true;
  cfg.use_inches = false;

  cfg.enable_logging = false;
  cfg.status_update_rate = 200;
}

void loadConfig() {
  prefs.begin("fluiddash", true);

  strlcpy(cfg.device_name, prefs.getString("dev_name", "fluiddash").c_str(), 32);
  strlcpy(cfg.fluidnc_ip, prefs.getString("fnc_ip", "192.168.73.13").c_str(), 16);
  cfg.fluidnc_port = prefs.getUShort("fnc_port", 81);  // FluidNC WebSocket default port
  cfg.fluidnc_auto_discover = prefs.getBool("fnc_auto", true);

  cfg.temp_threshold_low = prefs.getFloat("temp_low", 30.0);
  cfg.temp_threshold_high = prefs.getFloat("temp_high", 50.0);
  cfg.temp_offset_x = prefs.getFloat("cal_x", 0.0);
  cfg.temp_offset_yl = prefs.getFloat("cal_yl", 0.0);
  cfg.temp_offset_yr = prefs.getFloat("cal_yr", 0.0);
  cfg.temp_offset_z = prefs.getFloat("cal_z", 0.0);

  cfg.fan_min_speed = prefs.getUChar("fan_min", 30);
  cfg.fan_max_speed_limit = prefs.getUChar("fan_max", 100);

  cfg.psu_voltage_cal = prefs.getFloat("psu_cal", 7.3);
  cfg.psu_alert_low = prefs.getFloat("psu_low", 22.0);
  cfg.psu_alert_high = prefs.getFloat("psu_high", 26.0);

  cfg.brightness = prefs.getUChar("bright", 255);
  cfg.default_mode = (DisplayMode)prefs.getUChar("def_mode", 0);
  cfg.show_machine_coords = prefs.getBool("show_mpos", true);
  cfg.show_temp_graph = prefs.getBool("show_graph", true);
  cfg.coord_decimal_places = prefs.getUChar("coord_dec", 2);

  cfg.graph_timespan_seconds = prefs.getUShort("graph_time", 300);
  cfg.graph_update_interval = prefs.getUShort("graph_int", 5);

  cfg.use_fahrenheit = prefs.getBool("use_f", false);
  cfg.use_inches = prefs.getBool("use_in", false);

  cfg.enable_logging = prefs.getBool("logging", false);
  cfg.status_update_rate = prefs.getUShort("status_rate", 200);

  prefs.end();

  Serial.println("Configuration loaded");
}

void saveConfig() {
  prefs.begin("fluiddash", false);

  prefs.putString("dev_name", cfg.device_name);
  prefs.putString("fnc_ip", cfg.fluidnc_ip);
  prefs.putUShort("fnc_port", cfg.fluidnc_port);
  prefs.putBool("fnc_auto", cfg.fluidnc_auto_discover);

  prefs.putFloat("temp_low", cfg.temp_threshold_low);
  prefs.putFloat("temp_high", cfg.temp_threshold_high);
  prefs.putFloat("cal_x", cfg.temp_offset_x);
  prefs.putFloat("cal_yl", cfg.temp_offset_yl);
  prefs.putFloat("cal_yr", cfg.temp_offset_yr);
  prefs.putFloat("cal_z", cfg.temp_offset_z);

  prefs.putUChar("fan_min", cfg.fan_min_speed);
  prefs.putUChar("fan_max", cfg.fan_max_speed_limit);

  prefs.putFloat("psu_cal", cfg.psu_voltage_cal);
  prefs.putFloat("psu_low", cfg.psu_alert_low);
  prefs.putFloat("psu_high", cfg.psu_alert_high);

  prefs.putUChar("bright", cfg.brightness);
  prefs.putUChar("def_mode", cfg.default_mode);
  prefs.putBool("show_mpos", cfg.show_machine_coords);
  prefs.putBool("show_graph", cfg.show_temp_graph);
  prefs.putUChar("coord_dec", cfg.coord_decimal_places);

  prefs.putUShort("graph_time", cfg.graph_timespan_seconds);
  prefs.putUShort("graph_int", cfg.graph_update_interval);

  prefs.putBool("use_f", cfg.use_fahrenheit);
  prefs.putBool("use_in", cfg.use_inches);

  prefs.putBool("logging", cfg.enable_logging);
  prefs.putUShort("status_rate", cfg.status_update_rate);

  prefs.end();

  Serial.println("Configuration saved");
}
