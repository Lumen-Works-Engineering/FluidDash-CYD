#ifndef SD_MUTEX_H
#define SD_MUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t g_sdCardMutex;

void initSDMutex();

#endif // SD_MUTEX_H
