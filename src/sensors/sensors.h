#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <vector>

// ========== Sensor Mapping Structures ==========
struct SensorMapping {
    uint8_t uid[8];           // 64-bit DS18B20 ROM address
    char friendlyName[32];    // "X-Axis Motor" (using char[] instead of String for embedded)
    char alias[16];           // "temp0" (using char[] to reduce heap fragmentation)
    bool enabled;
    char notes[64];           // Optional user notes
};

// ========== Temperature Monitoring ==========
// Read temperature sensors (DS18B20 OneWire on CYD)
void readTemperatures();

// Calculate temperature from thermistor ADC value (legacy - for future use)
float calculateThermistorTemp(float adcValue);

// Update temperature history buffer
void updateTempHistory();

// ========== Fan Control ==========
// Control fan speed based on temperature
void controlFan();

// Calculate fan RPM from tachometer pulses
void calculateRPM();

// Tachometer interrupt handler
void IRAM_ATTR tachISR();

// ========== PSU Monitoring ==========
// Non-blocking sensor sampling (ADC for PSU voltage)
void sampleSensorsNonBlocking();

// Process averaged ADC readings
void processAdcReadings();

// ========== Sensor Management Functions ==========
// Initialize DS18B20 sensors
void initDS18B20Sensors();

// Load sensor configuration from SD card
void loadSensorConfig();

// Save sensor configuration to SD card
void saveSensorConfig();

// Get temperature by sensor alias (e.g., "temp0")
float getTempByAlias(const char* alias);

// Get temperature by UID
float getTempByUID(const uint8_t uid[8]);

// Get number of configured sensors
int getSensorCount();

// Add or update sensor mapping
bool addSensorMapping(const uint8_t uid[8], const char* name, const char* alias);

// Remove sensor mapping by alias
bool removeSensorMapping(const char* alias);

// Discover all DS18B20 sensors on the bus
std::vector<String> getDiscoveredUIDs();

// Convert UID to hex string
String uidToString(const uint8_t uid[8]);

// Convert hex string to UID
void stringToUID(const String& str, uint8_t uid[8]);

// Detect touched sensor (temperature rise detection)
String detectTouchedSensor(unsigned long timeoutMs, float thresholdDelta = 1.0);

// ========== External Variables ==========
// These are defined in main.cpp and accessed by sensor functions
extern float temperatures[4];
extern float peakTemps[4];
extern float psuVoltage;
extern float psuMin;
extern float psuMax;
extern uint8_t fanSpeed;
extern uint16_t fanRPM;
extern volatile uint16_t tachCounter;

// ADC sampling variables
extern uint32_t adcSamples[5][10];
extern uint8_t adcSampleIndex;
extern uint8_t adcCurrentSensor;
extern unsigned long lastAdcSample;
extern bool adcReady;

// Temperature history
extern float *tempHistory;
extern uint16_t historySize;
extern uint16_t historyIndex;

// Timing
extern unsigned long lastTachRead;

// Sensor mappings vector
extern std::vector<SensorMapping> sensorMappings;

#endif // SENSORS_H
