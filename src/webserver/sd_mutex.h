#ifndef SD_MUTEX_H
#define SD_MUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Arduino.h>

/**
 * SD Card Mutex for thread-safe SD operations
 *
 * Protects SD card access from concurrent access by:
 * - AsyncWebServer handlers (background tasks)
 * - Main loop operations (display, sensors, config)
 * - WebSocket handlers
 *
 * Usage:
 *   if (!SD_MUTEX_LOCK()) {
 *       // Handle error - mutex unavailable
 *       return;
 *   }
 *   File f = SD.open("/path");
 *   // ... do SD operations ...
 *   f.close();
 *   SD_MUTEX_UNLOCK();
 *
 * IMPORTANT: Always check SD_MUTEX_LOCK() return value!
 * Always unlock in the same function where you lock!
 */

extern SemaphoreHandle_t g_sdCardMutex;

void initSDMutex();

// Safe locking with NULL check and timeout - returns bool for error handling
inline bool SD_MUTEX_LOCK() {
    if (g_sdCardMutex == NULL) {
        Serial.println("[SD_MUTEX] ERROR: Mutex not initialized!");
        return false;
    }

    // 5-second timeout to prevent deadlock
    if (xSemaphoreTake(g_sdCardMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("[SD_MUTEX] ERROR: Timeout acquiring mutex!");
        return false;
    }

    return true;
}

inline void SD_MUTEX_UNLOCK() {
    if (g_sdCardMutex != NULL) {
        xSemaphoreGive(g_sdCardMutex);
    } else {
        Serial.println("[SD_MUTEX] WARNING: Attempted to unlock NULL mutex!");
    }
}

#endif // SD_MUTEX_H
