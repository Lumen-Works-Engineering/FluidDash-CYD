#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <Arduino.h>

/**
 * @brief Web request handlers for HTTP endpoints
 *
 * This module contains all HTTP request handler functions
 * for the web server.
 */

// ========== Page Handlers ==========

/**
 * @brief Handle root page request (/)
 */
void handleRoot();

/**
 * @brief Handle settings page request (/settings)
 */
void handleSettings();

/**
 * @brief Handle admin page request (/admin)
 */
void handleAdmin();

/**
 * @brief Handle WiFi configuration page request (/wifi)
 */
void handleWiFi();

/**
 * @brief Handle upload page request (/upload)
 */
void handleUpload();

/**
 * @brief Handle editor page request (/editor)
 */
void handleEditor();

// ========== API Handlers (GET) ==========

/**
 * @brief Handle API config request (GET /api/config)
 */
void handleAPIConfig();

/**
 * @brief Handle API status request (GET /api/status)
 */
void handleAPIStatus();

/**
 * @brief Handle API RTC request (GET /api/rtc)
 */
void handleAPIRTC();

/**
 * @brief Handle API upload status request (GET /api/upload-status)
 */
void handleUploadStatus();

/**
 * @brief Handle get JSON file request (GET /get-json)
 */
void handleGetJSON();

// ========== API Handlers (POST) ==========

/**
 * @brief Handle API save settings request (POST /api/save)
 */
void handleAPISave();

/**
 * @brief Handle API admin save request (POST /api/admin/save)
 */
void handleAPIAdminSave();

/**
 * @brief Handle API reset WiFi request (POST /api/reset-wifi)
 */
void handleAPIResetWiFi();

/**
 * @brief Handle API restart request (POST /api/restart)
 */
void handleAPIRestart();

/**
 * @brief Handle API reboot request (GET /api/reboot)
 */
void handleAPIReboot();

/**
 * @brief Handle API WiFi connect request (POST /api/wifi/connect)
 */
void handleAPIWiFiConnect();

/**
 * @brief Handle API reload screens request (POST /api/reload-screens)
 */
void handleAPIReloadScreens();

/**
 * @brief Handle API RTC set request (POST /api/rtc/set)
 */
void handleAPIRTCSet();

/**
 * @brief Handle save JSON file request (POST /save-json)
 */
void handleSaveJSON();

// ========== Upload Handlers ==========

/**
 * @brief Handle JSON file upload (multipart upload handler)
 */
void handleUploadJSON();

/**
 * @brief Handle upload complete callback
 */
void handleUploadComplete();

#endif // WEB_HANDLERS_H
