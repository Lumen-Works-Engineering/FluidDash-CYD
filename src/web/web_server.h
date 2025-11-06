#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

/**
 * @brief Web server setup and initialization
 *
 * This module handles web server initialization and route registration.
 */

/**
 * @brief Setup and start the web server
 *
 * Registers all HTTP routes and starts the web server.
 * Should be called once during setup().
 */
void setupWebServer();

#endif // WEB_SERVER_H
