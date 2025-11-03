#include "sensors.h"
#include "config/pins.h"
#include "config/config.h"
#include <Arduino.h>

// ========== Temperature Monitoring ==========

// Legacy function - now just calls non-blocking version
// Kept for compatibility but processing now happens in loop via non-blocking functions
void readTemperatures() {
  // This function kept for compatibility but processing now happens in loop
}

// Calculate temperature from thermistor ADC value using Steinhart-Hart equation
// This is legacy code for thermistor-based temperature sensing
// CYD uses DS18B20 OneWire sensors instead
float calculateThermistorTemp(float adcValue) {
  float voltage = (adcValue / ADC_RESOLUTION) * 3.3;
  if (voltage <= 0.01) return 0.0;  // Prevent division by zero

  float resistance = SERIES_RESISTOR * (3.3 / voltage - 1.0);
  float steinhart = resistance / THERMISTOR_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}

// Update temperature history buffer with max temperature
void updateTempHistory() {
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) {
      maxTemp = temperatures[i];
    }
  }

  tempHistory[historyIndex] = maxTemp;
  historyIndex = (historyIndex + 1) % historySize;
}

// ========== Fan Control ==========

// Control fan speed based on maximum temperature
// Maps temperature between low and high thresholds to fan speed range
void controlFan() {
  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) {
      maxTemp = temperatures[i];
    }
  }

  if (maxTemp < cfg.temp_threshold_low) {
    fanSpeed = cfg.fan_min_speed;
  } else if (maxTemp > cfg.temp_threshold_high) {
    fanSpeed = cfg.fan_max_speed_limit;
  } else {
    fanSpeed = map(maxTemp * 100, cfg.temp_threshold_low * 100,
                   cfg.temp_threshold_high * 100,
                   cfg.fan_min_speed, cfg.fan_max_speed_limit);
  }

  uint8_t pwmValue = map(fanSpeed, 0, 100, 0, 255);
  ledcWrite(0, pwmValue);  // channel 0
}

// Calculate fan RPM from tachometer pulses
// Assumes tachISR() is incrementing tachCounter on each pulse
// Most fans output 2 pulses per revolution
void calculateRPM() {
  fanRPM = (tachCounter * 60) / 2;
  tachCounter = 0;
}

// Tachometer interrupt handler (IRAM for fast execution)
// Defined in main.cpp - this declaration is here for reference
// void IRAM_ATTR tachISR() {
//   tachCounter++;
// }

// ========== PSU Monitoring ==========

// Non-blocking sensor sampling - call this repeatedly in loop()
// Samples PSU voltage ADC every 5ms and averages 10 samples
void sampleSensorsNonBlocking() {
  if (millis() - lastAdcSample < 5) {
    return;  // Sample every 5ms
  }
  lastAdcSample = millis();

  // CYD NOTE: Only PSU voltage is ADC-based now (temperatures use DS18B20 OneWire)
  // Take one sample from PSU voltage sensor only
  adcSamples[4][adcSampleIndex] = analogRead(PSU_VOLT);  // Index 4 = PSU voltage

  // Move to next sample
  adcSampleIndex++;
  if (adcSampleIndex >= 10) {
    adcSampleIndex = 0;
    adcReady = true;  // PSU voltage sampling complete
  }
}

// Process averaged ADC readings (called when adcReady is true)
// Calculates PSU voltage from averaged ADC samples
void processAdcReadings() {
  // CYD NOTE: Thermistor processing disabled - CYD uses DS18B20 OneWire sensors
  // TODO: Implement DS18B20 OneWire temperature reading for CYD
  // For now, set dummy temperature values to prevent display errors
  for (int sensor = 0; sensor < 4; sensor++) {
    temperatures[sensor] = 25.0;  // Placeholder: 25C room temperature
    peakTemps[sensor] = 25.0;
  }

  // Process PSU voltage
  uint32_t sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += adcSamples[4][i];
  }
  float adcValue = sum / 10.0;
  float measuredVoltage = (adcValue / ADC_RESOLUTION) * 3.3;
  psuVoltage = measuredVoltage * cfg.psu_voltage_cal;

  if (psuVoltage < psuMin && psuVoltage > 10.0) psuMin = psuVoltage;
  if (psuVoltage > psuMax) psuMax = psuVoltage;
}
