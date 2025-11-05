#include "sd_mutex.h"
#include <Arduino.h>

SemaphoreHandle_t g_sdCardMutex = NULL;

void initSDMutex() {
    Serial.printf("[SD_MUTEX] Creating mutex...\n");

    g_sdCardMutex = xSemaphoreCreateMutex();

    if (g_sdCardMutex == NULL) {
        Serial.println("[SD_MUTEX] FATAL: xSemaphoreCreateMutex() failed!");
        return;
    }

    Serial.printf("[SD_MUTEX] ✓ Mutex created at 0x%p\n", g_sdCardMutex);

    // Test it immediately
    if (xSemaphoreTake(g_sdCardMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        xSemaphoreGive(g_sdCardMutex);
        Serial.println("[SD_MUTEX] ✓ Mutex test PASSED");
    } else {
        Serial.println("[SD_MUTEX] ✗ Mutex test FAILED - cannot acquire!");
    }
}
