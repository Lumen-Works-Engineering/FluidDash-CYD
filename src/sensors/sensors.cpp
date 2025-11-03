#include "sensors.h"
#include "config/pins.h"
#include "config/config.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ========== DS18B20 OneWire Setup ==========
OneWire oneWire(ONE_WIRE_BUS_1);
DallasTemperature ds18b20Sensors(&oneWire);

// Sensor mappings vector (stores UID to friendly name mappings)
std::vector<SensorMapping> sensorMappings;

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
// Calculates PSU voltage from averaged ADC samples and reads DS18B20 sensors
void processAdcReadings() {
  // Read DS18B20 temperature sensors
  ds18b20Sensors.requestTemperatures();  // Request readings from all sensors

  // Update temperatures array with readings from mapped sensors
  int deviceCount = ds18b20Sensors.getDeviceCount();

  // Clear temperatures array first
  for (int i = 0; i < 4; i++) {
    temperatures[i] = 0.0;
  }

  // Read temperatures based on sensor mappings
  for (size_t i = 0; i < sensorMappings.size() && i < 4; i++) {
    if (sensorMappings[i].enabled) {
      float temp = ds18b20Sensors.getTempC(sensorMappings[i].uid);
      if (temp != DEVICE_DISCONNECTED_C && temp > -55.0 && temp < 125.0) {
        temperatures[i] = temp;
        if (temp > peakTemps[i]) {
          peakTemps[i] = temp;
        }
      }
    }
  }

  // If no mappings exist yet, read first 4 discovered sensors directly
  if (sensorMappings.empty() && deviceCount > 0) {
    for (int i = 0; i < min(deviceCount, 4); i++) {
      float temp = ds18b20Sensors.getTempCByIndex(i);
      if (temp != DEVICE_DISCONNECTED_C && temp > -55.0 && temp < 125.0) {
        temperatures[i] = temp;
        if (temp > peakTemps[i]) {
          peakTemps[i] = temp;
        }
      }
    }
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

// ========== Sensor Management Functions ==========

// Initialize DS18B20 sensors on OneWire bus
void initDS18B20Sensors() {
  Serial.println("[SENSORS] Initializing DS18B20 sensors...");

  ds18b20Sensors.begin();
  int deviceCount = ds18b20Sensors.getDeviceCount();

  Serial.printf("[SENSORS] Found %d DS18B20 sensor(s) on bus\n", deviceCount);

  // Set resolution to 12-bit for all sensors (0.0625°C precision)
  ds18b20Sensors.setResolution(12);

  // Set wait for conversion to false for non-blocking operation
  ds18b20Sensors.setWaitForConversion(false);

  // Print discovered sensor UIDs
  for (int i = 0; i < deviceCount; i++) {
    uint8_t addr[8];
    if (ds18b20Sensors.getAddress(addr, i)) {
      Serial.printf("[SENSORS] Sensor %d UID: ", i);
      for (int j = 0; j < 8; j++) {
        Serial.printf("%02X", addr[j]);
        if (j < 7) Serial.print(":");
      }
      Serial.println();
    }
  }

  Serial.println("[SENSORS] DS18B20 initialization complete");
}

// Get sensor count from mappings
int getSensorCount() {
  return sensorMappings.size();
}

// Get temperature by alias (e.g., "temp0")
float getTempByAlias(const char* alias) {
  for (const auto& mapping : sensorMappings) {
    if (strcmp(mapping.alias, alias) == 0 && mapping.enabled) {
      float temp = ds18b20Sensors.getTempC(mapping.uid);
      if (temp != DEVICE_DISCONNECTED_C && temp > -55.0 && temp < 125.0) {
        return temp;
      }
    }
  }
  return NAN;  // Return NaN if sensor not found or invalid reading
}

// Get temperature by UID
float getTempByUID(const uint8_t uid[8]) {
  float temp = ds18b20Sensors.getTempC(uid);
  if (temp != DEVICE_DISCONNECTED_C && temp > -55.0 && temp < 125.0) {
    return temp;
  }
  return NAN;
}
