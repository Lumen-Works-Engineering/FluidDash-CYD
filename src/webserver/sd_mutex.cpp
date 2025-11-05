#include "sd_mutex.h"

// SD card mutex - initialized in initSDMutex()
SemaphoreHandle_t g_sdCardMutex = NULL;

void initSDMutex() {
    if (g_sdCardMutex != NULL) {
        Serial.println("[SD_MUTEX] WARNING: Mutex already initialized!");
        return;
    }

    // Use counting semaphore initialized to 1 (works like mutex but ISR-safe)
    g_sdCardMutex = xSemaphoreCreateCounting(1, 1);

    if (g_sdCardMutex != NULL) {
        Serial.println("[SD_MUTEX] Mutex initialized successfully");
    } else {
        Serial.println("[SD_MUTEX] FATAL ERROR: Failed to create semaphore!");
    }
}
