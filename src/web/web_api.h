#ifndef WEB_API_H
#define WEB_API_H

#include <Arduino.h>

/**
 * @brief Web API JSON response generators
 *
 * This module provides functions to generate JSON responses
 * for the REST API endpoints.
 */

/**
 * @brief Generate configuration JSON response
 * @return JSON string with current configuration
 */
String getConfigJSON();

/**
 * @brief Generate status JSON response
 * @return JSON string with current system status
 */
String getStatusJSON();

/**
 * @brief Generate RTC time JSON response
 * @return JSON string with current RTC time
 */
String getRTCJSON();

/**
 * @brief Generate upload status JSON response
 * @return JSON string with storage status
 */
String getUploadStatusJSON();

#endif // WEB_API_H
