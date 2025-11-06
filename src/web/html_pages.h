#ifndef HTML_PAGES_H
#define HTML_PAGES_H

#include <Arduino.h>

// External references to HTML templates defined in main.cpp
extern const char MAIN_HTML[] PROGMEM;
extern const char SETTINGS_HTML[] PROGMEM;
extern const char ADMIN_HTML[] PROGMEM;
extern const char WIFI_CONFIG_HTML[] PROGMEM;

// Function declarations
String getMainHTML();
String getSettingsHTML();
String getAdminHTML();
String getWiFiConfigHTML();

#endif // HTML_PAGES_H
