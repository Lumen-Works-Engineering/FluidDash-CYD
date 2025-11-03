#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

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

#endif // SENSORS_H
