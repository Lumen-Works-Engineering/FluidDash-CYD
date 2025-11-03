#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// ========== Memory Management ==========
// Allocate temperature history buffer based on config
void allocateHistoryBuffer();

// ========== Watchdog Functions ==========
// Enable ESP32 watchdog timer
void enableLoopWDT();

// Feed the watchdog to prevent reset
void feedLoopWDT();

// ========== External Variables ==========
extern float *tempHistory;
extern uint16_t historySize;
extern uint16_t historyIndex;

#endif // UTILS_H
