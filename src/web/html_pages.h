#ifndef HTML_PAGES_H
#define HTML_PAGES_H

#include <Arduino.h>

/**
 * @brief HTML page generators
 *
 * This module provides functions to generate HTML pages with
 * dynamic content replacement.
 *
 * NOTE: Currently uses PROGMEM strings. Future enhancement:
 * Load HTML templates from SPIFFS/SD card for easier editing.
 */

/**
 * @brief Get main dashboard HTML page
 * @return HTML string with placeholders replaced
 */
String getMainHTML();

/**
 * @brief Get settings configuration HTML page
 * @return HTML string with placeholders replaced
 */
String getSettingsHTML();

/**
 * @brief Get admin/calibration HTML page
 * @return HTML string with placeholders replaced
 */
String getAdminHTML();

/**
 * @brief Get WiFi configuration HTML page
 * @return HTML string with placeholders replaced
 */
String getWiFiConfigHTML();

#endif // HTML_PAGES_H
