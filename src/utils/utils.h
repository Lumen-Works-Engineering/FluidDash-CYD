#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// ========== Memory Management ==========
// Allocate temperature history buffer based on config
void allocateHistoryBuffer();

// ========== Watchdog Functions ==========
// Note: enableLoopWDT() and feedLoopWDT() are provided by the ESP32 Arduino framework
// They are declared in esp32-hal.h and don't need to be redeclared here

// ========== External Variables ==========
extern float *tempHistory;
extern uint16_t historySize;
extern uint16_t historyIndex;

#endif // UTILS_H
