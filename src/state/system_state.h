#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>

/**
 * @brief System state management class
 *
 * Encapsulates all system-level runtime state including:
 * - Temperature sensors
 * - PSU voltage monitoring
 * - Fan control
 * - Display mode
 * - Timing variables
 * - Hardware availability flags
 */
class SystemState
{
public:
    // ========== Temperature Monitoring ==========
    float temperatures[4]; // Current temperatures (°C) [0-3]
    float peakTemps[4];    // Peak temperatures (°C) [0-3]
    float *tempHistory;    // Dynamic array for temperature graph
    uint16_t historySize;  // Size of tempHistory buffer
    uint16_t historyIndex; // Current index in circular buffer

    // ========== Fan Control ==========
    volatile uint16_t tachCounter; // Fan tachometer pulse counter
    uint16_t fanRPM;               // Fan speed in RPM
    uint8_t fanSpeed;              // Fan speed percentage (0-100%)

    // ========== Power Supply Monitoring ==========
    float psuVoltage; // Current PSU voltage (V)
    float psuMin;     // Minimum recorded PSU voltage (V)
    float psuMax;     // Maximum recorded PSU voltage (V)

    // ========== ADC Sampling (Non-blocking) ==========
    uint32_t adcSamples[5][10];  // ADC sample buffer (4 temps + PSU, 10 samples each)
    uint8_t adcSampleIndex;      // Current ADC sample index
    uint8_t adcCurrentSensor;    // Current ADC sensor being sampled
    unsigned long lastAdcSample; // Last ADC sample timestamp
    bool adcReady;               // ADC averaging complete flag

    // ========== Display & UI ==========
    DisplayMode currentMode;         // Current display mode
    unsigned long lastDisplayUpdate; // Last display refresh timestamp
    unsigned long lastHistoryUpdate; // Last history update timestamp
    unsigned long buttonPressStart;  // Button press start timestamp
    bool buttonPressed;              // Button press state

    // ========== Hardware Availability ==========
    bool sdCardAvailable; // SD card availability flag
    bool rtcAvailable;    // RTC module availability flag
    bool inAPMode;        // WiFi AP mode flag

    // ========== Timing ==========
    unsigned long sessionStartTime; // Session start timestamp (millis)
    unsigned long lastTachRead;     // Last tachometer read timestamp

    // ========== Constructor ==========
    SystemState();

    // ========== Methods ==========

    /**
     * @brief Initialize system state with default values
     */
    void init();

    /**
     * @brief Reset peak temperatures
     */
    void resetPeakTemps();

    /**
     * @brief Reset PSU min/max values
     */
    void resetPSUMinMax();

    /**
     * @brief Get maximum current temperature
     * @return Maximum temperature from all sensors
     */
    float getMaxTemp() const;

    /**
     * @brief Update peak temperatures if current exceeds peak
     */
    void updatePeakTemps();

    /**
     * @brief Update PSU min/max if current exceeds limits
     */
    void updatePSUMinMax();

    /**
     * @brief Allocate temperature history buffer
     * @param size Number of history points to allocate
     * @return true if allocation successful
     */
    bool allocateTempHistory(uint16_t size);

    /**
     * @brief Free temperature history buffer
     */
    void freeTempHistory();

    /**
     * @brief Add temperature reading to history
     * @param temp Temperature value to add
     */
    void addTempToHistory(float temp);
};

// Global system state instance
extern SystemState systemState;

#endif // SYSTEM_STATE_H
