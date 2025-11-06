#include "system_state.h"
#include "config/config.h"
#include <cstring>

// Global instance
SystemState systemState;

SystemState::SystemState()
{
    init();
}

void SystemState::init()
{
    // Initialize temperatures
    memset(temperatures, 0, sizeof(temperatures));
    memset(peakTemps, 0, sizeof(peakTemps));
    tempHistory = nullptr;
    historySize = 0;
    historyIndex = 0;

    // Initialize fan control
    tachCounter = 0;
    fanRPM = 0;
    fanSpeed = 0;

    // Initialize PSU monitoring
    psuVoltage = 0.0f;
    psuMin = 99.9f;
    psuMax = 0.0f;

    // Initialize ADC sampling
    memset(adcSamples, 0, sizeof(adcSamples));
    adcSampleIndex = 0;
    adcCurrentSensor = 0;
    lastAdcSample = 0;
    adcReady = false;

    // Initialize display & UI
    currentMode = MODE_MONITOR;
    lastDisplayUpdate = 0;
    lastHistoryUpdate = 0;
    buttonPressStart = 0;
    buttonPressed = false;

    // Initialize hardware availability
    sdCardAvailable = false;
    rtcAvailable = false;
    inAPMode = false;

    // Initialize timing
    sessionStartTime = 0;
    lastTachRead = 0;
}

void SystemState::resetPeakTemps()
{
    memset(peakTemps, 0, sizeof(peakTemps));
}

void SystemState::resetPSUMinMax()
{
    psuMin = 99.9f;
    psuMax = 0.0f;
}

float SystemState::getMaxTemp() const
{
    float maxTemp = temperatures[0];
    for (int i = 1; i < 4; i++)
    {
        if (temperatures[i] > maxTemp)
        {
            maxTemp = temperatures[i];
        }
    }
    return maxTemp;
}

void SystemState::updatePeakTemps()
{
    for (int i = 0; i < 4; i++)
    {
        if (temperatures[i] > peakTemps[i])
        {
            peakTemps[i] = temperatures[i];
        }
    }
}

void SystemState::updatePSUMinMax()
{
    if (psuVoltage < psuMin && psuVoltage > 0.0f)
    {
        psuMin = psuVoltage;
    }
    if (psuVoltage > psuMax)
    {
        psuMax = psuVoltage;
    }
}

bool SystemState::allocateTempHistory(uint16_t size)
{
    // Free existing buffer if any
    freeTempHistory();

    // Allocate new buffer
    tempHistory = (float *)malloc(size * sizeof(float));
    if (tempHistory == nullptr)
    {
        historySize = 0;
        return false;
    }

    // Initialize buffer to zero
    memset(tempHistory, 0, size * sizeof(float));
    historySize = size;
    historyIndex = 0;

    return true;
}

void SystemState::freeTempHistory()
{
    if (tempHistory != nullptr)
    {
        free(tempHistory);
        tempHistory = nullptr;
    }
    historySize = 0;
    historyIndex = 0;
}

void SystemState::addTempToHistory(float temp)
{
    if (tempHistory == nullptr || historySize == 0)
    {
        return;
    }

    // Add temperature to circular buffer
    tempHistory[historyIndex] = temp;
    historyIndex = (historyIndex + 1) % historySize;
}
