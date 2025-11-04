# FluidDash Dynamic Data Variables & API Reference

## Document Purpose

This document catalogs all dynamic screen elements (runtime variables) that can be displayed on the screen and all available API endpoints for web/remote access. These variables are updated in real-time from sensors, FluidNC CNC controller, and system status.

---

## Table of Contents

1. [Dynamic Screen Variables](#dynamic-screen-variables)
2. [Variable Categories](#variable-categories)
3. [Data Source Identifiers for JSON Layouts](#data-source-identifiers-for-json-layouts)
4. [API Endpoints](#api-endpoints)
5. [JSON Response Formats](#json-response-formats)
6. [Configuration Variables](#configuration-variables)
7. [Usage Examples](#usage-examples)

---

## Dynamic Screen Variables

These are the runtime variables defined in `src/main.cpp` that hold current system state and can be displayed on the screen.

### Location in Code

**File**: `src/main.cpp:66-113`

### Complete Variable List

```cpp
// Display & System Status
DisplayMode currentMode;              // Current display mode (MONITOR/POSITION/GRAPH)
bool sdCardAvailable;                 // SD card availability flag
bool inAPMode;                        // WiFi AP mode flag
bool rtcAvailable;                    // RTC module availability flag

// Temperature Monitoring
float temperatures[4];                // Current temperatures (°C) [0-3]
float peakTemps[4];                   // Peak temperatures (°C) [0-3]
float *tempHistory;                   // Dynamic array for temperature graph
uint16_t historySize;                 // Size of tempHistory buffer
uint16_t historyIndex;                // Current index in circular buffer

// Fan Control
volatile uint16_t tachCounter;        // Fan tachometer pulse counter
uint16_t fanRPM;                      // Fan speed in RPM
uint8_t fanSpeed;                     // Fan speed percentage (0-100%)

// Power Supply Monitoring
float psuVoltage;                     // Current PSU voltage (V)
float psuMin;                         // Minimum recorded PSU voltage (V)
float psuMax;                         // Maximum recorded PSU voltage (V)

// FluidNC CNC Status
String machineState;                  // Machine state: "IDLE", "RUN", "ALARM", "OFFLINE", etc.
bool fluidncConnected;                // FluidNC connection status
unsigned long jobStartTime;           // Job start timestamp (millis)
bool isJobRunning;                    // Job running flag

// Position Data (Machine Coordinates)
float posX, posY, posZ, posA;         // Machine position (MPos) in mm

// Position Data (Work Coordinates)
float wposX, wposY, wposZ, wposA;     // Work position (WPos) in mm

// Work Coordinate Offsets
float wcoX, wcoY, wcoZ, wcoA;         // Work coordinate offsets (WCO) in mm

// Motion Status
int feedRate;                         // Current feed rate (mm/min)
int spindleRPM;                       // Spindle speed (RPM)
int feedOverride;                     // Feed override percentage (default: 100%)
int rapidOverride;                    // Rapid override percentage (default: 100%)
int spindleOverride;                  // Spindle override percentage (default: 100%)

// Timing & Session
unsigned long sessionStartTime;       // Session start timestamp (millis)
unsigned long lastDisplayUpdate;      // Last display refresh timestamp
unsigned long lastHistoryUpdate;      // Last history update timestamp
unsigned long lastStatusRequest;      // Last status request timestamp
unsigned long lastTachRead;           // Last tachometer read timestamp
unsigned long buttonPressStart;       // Button press start timestamp
bool buttonPressed;                   // Button press state

// WebSocket & Reporting
bool autoReportingEnabled;            // Auto-reporting state flag
unsigned long reportingSetupTime;     // Reporting setup timestamp
bool debugWebSocket;                  // WebSocket debug flag

// ADC Sampling (Internal - not typically displayed)
uint32_t adcSamples[5][10];          // ADC sample buffer (4 temps + PSU, 10 samples each)
uint8_t adcSampleIndex;              // Current ADC sample index
uint8_t adcCurrentSensor;            // Current ADC sensor being sampled
unsigned long lastAdcSample;         // Last ADC sample timestamp
bool adcReady;                       // ADC averaging complete flag
```

---

## Variable Categories

### 1. Temperature Data

| Variable          | Type   | Range      | Units | Description                      | API Field         |
| ----------------- | ------ | ---------- | ----- | -------------------------------- | ----------------- |
| `temperatures[0]` | float  | -55 to 125 | °C    | Temperature sensor 0 (DS18B20)   | `temperatures[0]` |
| `temperatures[1]` | float  | -55 to 125 | °C    | Temperature sensor 1 (DS18B20)   | `temperatures[1]` |
| `temperatures[2]` | float  | -55 to 125 | °C    | Temperature sensor 2 (DS18B20)   | `temperatures[2]` |
| `temperatures[3]` | float  | -55 to 125 | °C    | Temperature sensor 3 (DS18B20)   | `temperatures[3]` |
| `peakTemps[0-3]`  | float  | -55 to 125 | °C    | Peak temperature for each sensor | (not in API)      |
| `tempHistory[]`   | float* | -55 to 125 | °C    | Circular buffer for graphing     | (not in API)      |

**Notes**:

- DS18B20 sensors provide 12-bit resolution (0.0625°C precision)
- Invalid/disconnected sensors return 0.0
- Peak temps reset on reboot
- Temperature history size configurable via `cfg.graph_timespan_seconds`

### 2. Cooling System

| Variable      | Type              | Range    | Units  | Description                   | API Field    |
| ------------- | ----------------- | -------- | ------ | ----------------------------- | ------------ |
| `fanSpeed`    | uint8_t           | 0-100    | %      | PWM fan speed percentage      | `fan_speed`  |
| `fanRPM`      | uint16_t          | 0-10000+ | RPM    | Measured fan tachometer speed | `fan_rpm`    |
| `tachCounter` | volatile uint16_t | 0-65535  | pulses | Pulse counter (internal)      | (not in API) |

**Notes**:

- Fan speed controlled by temperature thresholds (see Configuration)
- RPM calculated from tachometer pulses (2 pulses per revolution)
- Tachometer on GPIO pin `FAN_TACH`

### 3. Power Supply

| Variable     | Type  | Range | Units | Description             | API Field     |
| ------------ | ----- | ----- | ----- | ----------------------- | ------------- |
| `psuVoltage` | float | 0-30+ | V     | Current PSU voltage     | `psu_voltage` |
| `psuMin`     | float | 0-30+ | V     | Session minimum voltage | (not in API)  |
| `psuMax`     | float | 0-30+ | V     | Session maximum voltage | (not in API)  |

**Notes**:

- ADC-based voltage measurement with calibration factor
- 10-sample averaging for noise reduction
- Min/Max reset on reboot

### 4. Machine Position (CNC)

#### Machine Coordinates (MPos)

| Variable | Type  | Range    | Units  | Description             | API Field    |
| -------- | ----- | -------- | ------ | ----------------------- | ------------ |
| `posX`   | float | -∞ to +∞ | mm     | X-axis machine position | `mpos_x`     |
| `posY`   | float | -∞ to +∞ | mm     | Y-axis machine position | `mpos_y`     |
| `posZ`   | float | -∞ to +∞ | mm     | Z-axis machine position | `mpos_z`     |
| `posA`   | float | -∞ to +∞ | deg/mm | A-axis machine position | (not in API) |

#### Work Coordinates (WPos)

| Variable | Type  | Range    | Units  | Description          | API Field    |
| -------- | ----- | -------- | ------ | -------------------- | ------------ |
| `wposX`  | float | -∞ to +∞ | mm     | X-axis work position | `wpos_x`     |
| `wposY`  | float | -∞ to +∞ | mm     | Y-axis work position | `wpos_y`     |
| `wposZ`  | float | -∞ to +∞ | mm     | Z-axis work position | `wpos_z`     |
| `wposA`  | float | -∞ to +∞ | deg/mm | A-axis work position | (not in API) |

#### Work Coordinate Offsets (WCO)

| Variable | Type  | Range    | Units  | Description        | API Field    |
| -------- | ----- | -------- | ------ | ------------------ | ------------ |
| `wcoX`   | float | -∞ to +∞ | mm     | X-axis work offset | (not in API) |
| `wcoY`   | float | -∞ to +∞ | mm     | Y-axis work offset | (not in API) |
| `wcoZ`   | float | -∞ to +∞ | mm     | Z-axis work offset | (not in API) |
| `wcoA`   | float | -∞ to +∞ | deg/mm | A-axis work offset | (not in API) |

**Relationship**: `WPos = MPos - WCO`

**Notes**:

- Updated via WebSocket from FluidNC controller
- Precision configurable via `cfg.coord_decimal_places` (0-4)
- Can display in inches if `cfg.use_inches = true`

### 5. Machine Status (CNC)

| Variable           | Type   | Values      | Description               | API Field       |
| ------------------ | ------ | ----------- | ------------------------- | --------------- |
| `machineState`     | String | See below   | Current machine state     | `machine_state` |
| `fluidncConnected` | bool   | true/false  | FluidNC connection status | `connected`     |
| `isJobRunning`     | bool   | true/false  | Job running flag          | (not in API)    |
| `jobStartTime`     | ulong  | 0 to 2^32-1 | Job start time (millis)   | (not in API)    |

**Machine State Values**:

- `"IDLE"` - Machine ready, not running
- `"RUN"` - Actively executing G-code
- `"HOLD"` - Feed hold active
- `"ALARM"` - Alarm state (homing required, limit triggered, etc.)
- `"CHECK"` - G-code check mode
- `"DOOR"` - Safety door open
- `"HOME"` - Homing cycle in progress
- `"JOG"` - Jogging mode
- `"SLEEP"` - Sleep mode
- `"OFFLINE"` - Not connected to FluidNC

**Color Coding in UI**:

- `RUN` → Green (`COLOR_GOOD`)
- `ALARM` → Red (`COLOR_WARN`)
- Other → White/default

### 6. Motion & Spindle

| Variable          | Type | Range    | Units  | Description            | API Field    |
| ----------------- | ---- | -------- | ------ | ---------------------- | ------------ |
| `feedRate`        | int  | 0-10000+ | mm/min | Programmed feed rate   | (not in API) |
| `spindleRPM`      | int  | 0-30000+ | RPM    | Spindle speed          | (not in API) |
| `feedOverride`    | int  | 10-200   | %      | Feed rate override     | (not in API) |
| `rapidOverride`   | int  | 25-100   | %      | Rapid rate override    | (not in API) |
| `spindleOverride` | int  | 10-200   | %      | Spindle speed override | (not in API) |

**Notes**:

- Overrides default to 100% (no modification)
- Feed/spindle overrides adjustable in real-time
- Rapid override limited for safety (typically 25-100%)

### 7. System & Network

| Variable          | Type      | Values     | Description            | Data Source ID |
| ----------------- | --------- | ---------- | ---------------------- | -------------- |
| WiFi.localIP()    | IPAddress | x.x.x.x    | Device IP address      | `ipAddress`    |
| WiFi.SSID()       | String    | text       | Connected WiFi network | `ssid`         |
| `cfg.device_name` | char[32]  | text       | Device friendly name   | `deviceName`   |
| `cfg.fluidnc_ip`  | char[32]  | x.x.x.x    | FluidNC controller IP  | `fluidncIP`    |
| `sdCardAvailable` | bool      | true/false | SD card mounted        | (not directly) |
| `rtcAvailable`    | bool      | true/false | RTC module present     | (not directly) |

---

## Data Source Identifiers for JSON Layouts

When creating custom screen layouts in JSON (stored on SD card), use these **data source strings** to bind UI elements to live data. These are parsed in `src/display/screen_renderer.cpp`.

### Numeric Data Sources (float)

**Location**: `src/display/screen_renderer.cpp:185-207`

```cpp
float getDataValue(const char* dataSource)
```

| Data Source ID | Variable        | Type          | Units  | Description          |
| -------------- | --------------- | ------------- | ------ | -------------------- |
| `"posX"`       | posX            | float         | mm     | Machine X position   |
| `"posY"`       | posY            | float         | mm     | Machine Y position   |
| `"posZ"`       | posZ            | float         | mm     | Machine Z position   |
| `"posA"`       | posA            | float         | mm/deg | Machine A position   |
| `"wposX"`      | wposX           | float         | mm     | Work X position      |
| `"wposY"`      | wposY           | float         | mm     | Work Y position      |
| `"wposZ"`      | wposZ           | float         | mm     | Work Z position      |
| `"wposA"`      | wposA           | float         | mm/deg | Work A position      |
| `"feedRate"`   | feedRate        | int→float     | mm/min | Feed rate            |
| `"spindleRPM"` | spindleRPM      | int→float     | RPM    | Spindle speed        |
| `"psuVoltage"` | psuVoltage      | float         | V      | PSU voltage          |
| `"fanSpeed"`   | fanSpeed        | uint8_t→float | %      | Fan speed percentage |
| `"temp0"`      | temperatures[0] | float         | °C     | Temperature sensor 0 |
| `"temp1"`      | temperatures[1] | float         | °C     | Temperature sensor 1 |
| `"temp2"`      | temperatures[2] | float         | °C     | Temperature sensor 2 |
| `"temp3"`      | temperatures[3] | float         | °C     | Temperature sensor 3 |

**Returns**: `0.0f` if data source not found

### String Data Sources

**Location**: `src/display/screen_renderer.cpp:210-220`

```cpp
String getDataString(const char* dataSource)
```

| Data Source ID   | Variable        | Type   | Description        |
| ---------------- | --------------- | ------ | ------------------ |
| `"machineState"` | machineState    | String | Current CNC state  |
| `"ipAddress"`    | WiFi.localIP()  | String | Device IP address  |
| `"ssid"`         | WiFi.SSID()     | String | WiFi network name  |
| `"deviceName"`   | cfg.device_name | String | Device name        |
| `"fluidncIP"`    | cfg.fluidnc_ip  | String | FluidNC IP address |

**Fallback**: If data source matches a numeric variable, returns `String(value, 2)` (2 decimal places)

### Element Types for JSON Layouts

When creating custom screens, use these element types:

| Type         | String ID    | Description                      | Required Fields                   |
| ------------ | ------------ | -------------------------------- | --------------------------------- |
| Rectangle    | `"rect"`     | Filled or outline rectangle      | x, y, w, h, color, filled         |
| Line         | `"line"`     | Horizontal or vertical line      | x, y, w, h, color                 |
| Static Text  | `"text"`     | Fixed text label                 | x, y, label, textSize, color      |
| Dynamic Text | `"dynamic"`  | Data-driven text                 | x, y, dataSource, textSize, color |
| Temperature  | `"temp"`     | Temperature with unit conversion | x, y, dataSource, textSize, color |
| Coordinate   | `"coord"`    | Position with unit conversion    | x, y, dataSource, textSize, color |
| Status       | `"status"`   | Color-coded machine state        | x, y, dataSource, textSize        |
| Progress Bar | `"progress"` | Progress indicator               | x, y, w, h, color                 |
| Graph        | `"graph"`    | Mini temperature graph           | x, y, w, h, color                 |

**Example JSON Element**:

```json
{
  "type": "temp",
  "x": 10,
  "y": 50,
  "dataSource": "temp0",
  "textSize": 3,
  "color": "#00FF00",
  "showLabel": true,
  "label": "Motor: "
}
```

---

## API Endpoints

All API endpoints are defined in `src/main.cpp:891-1160` in the `setupWebServer()` function.

### Web Pages (HTML)

| Endpoint    | Method | Content Type | Description                 | Function              |
| ----------- | ------ | ------------ | --------------------------- | --------------------- |
| `/`         | GET    | text/html    | Main dashboard page         | `getMainHTML()`       |
| `/settings` | GET    | text/html    | Settings configuration page | `getSettingsHTML()`   |
| `/admin`    | GET    | text/html    | Admin/calibration page      | `getAdminHTML()`      |
| `/wifi`     | GET    | text/html    | WiFi configuration page     | `getWiFiConfigHTML()` |
| `/upload`   | GET    | text/html    | JSON file upload page       | (inline HTML)         |
| `/editor`   | GET    | text/html    | Live JSON editor            | (inline HTML)         |

### API Endpoints (JSON)

#### GET Endpoints (Read Data)

| Endpoint      | Method | Response Type    | Description                | Returns                         |
| ------------- | ------ | ---------------- | -------------------------- | ------------------------------- |
| `/api/config` | GET    | application/json | Get current configuration  | See [Config JSON](#config-json) |
| `/api/status` | GET    | application/json | Get current system status  | See [Status JSON](#status-json) |
| `/get-json`   | GET    | application/json | Get JSON file from SD card | File contents or error          |

**Query Parameters**:

- `/get-json?file=monitor.json` - Specify filename to retrieve

#### POST Endpoints (Write/Command)

| Endpoint              | Method | Parameters     | Description                 |
| --------------------- | ------ | -------------- | --------------------------- |
| `/api/save`           | POST   | See below      | Save settings configuration |
| `/api/admin/save`     | POST   | See below      | Save calibration settings   |
| `/api/reset-wifi`     | POST   | (none)         | Reset WiFi and restart      |
| `/api/restart`        | POST   | (none)         | Restart device              |
| `/api/wifi/connect`   | POST   | ssid, password | Connect to WiFi network     |
| `/api/reload-screens` | POST   | (none)         | Reload JSON layouts from SD |
| `/upload-json`        | POST   | file upload    | Upload JSON file to SD card |
| `/save-json`          | POST   | JSON body      | Save edited JSON to SD card |

### API Parameter Details

#### `/api/save` Parameters (Settings)

| Parameter        | Type  | Range   | Default | Description                     |
| ---------------- | ----- | ------- | ------- | ------------------------------- |
| `temp_low`       | float | 0-100   | 30.0    | Low temp threshold (°C)         |
| `temp_high`      | float | 0-100   | 50.0    | High temp threshold (°C)        |
| `fan_min`        | int   | 0-100   | 20      | Minimum fan speed (%)           |
| `graph_time`     | int   | 60-7200 | 3600    | Graph timespan (seconds)        |
| `graph_interval` | int   | 1-60    | 5       | Graph update interval (seconds) |
| `psu_low`        | float | 0-30    | 11.0    | PSU low voltage alert (V)       |
| `psu_high`       | float | 0-30    | 13.0    | PSU high voltage alert (V)      |
| `coord_decimals` | int   | 0-4     | 3       | Coordinate decimal places       |

**Note**: Changing `graph_time` triggers `allocateHistoryBuffer()` to reallocate memory.

#### `/api/admin/save` Parameters (Calibration)

| Parameter | Type  | Range       | Description                        |
| --------- | ----- | ----------- | ---------------------------------- |
| `cal_x`   | float | -50 to +50  | Temp 0 offset (°C)                 |
| `cal_yl`  | float | -50 to +50  | Temp 1 offset (°C)                 |
| `cal_yr`  | float | -50 to +50  | Temp 2 offset (°C)                 |
| `cal_z`   | float | -50 to +50  | Temp 3 offset (°C)                 |
| `psu_cal` | float | 0.1 to 10.0 | PSU voltage calibration multiplier |

#### `/api/wifi/connect` Parameters

| Parameter  | Type   | Max Length | Description       |
| ---------- | ------ | ---------- | ----------------- |
| `ssid`     | String | 32         | WiFi network SSID |
| `password` | String | 64         | WiFi password     |

---

## JSON Response Formats

### Config JSON

**Endpoint**: `/api/config`
**Location**: `src/main.cpp:1300-1313`

```json
{
  "device_name": "FluidDash",
  "fluidnc_ip": "192.168.1.100",
  "temp_low": 30.0,
  "temp_high": 50.0,
  "fan_min": 20,
  "psu_low": 11.0,
  "psu_high": 13.0,
  "graph_time": 3600,
  "graph_interval": 5
}
```

**Fields**:
| Field | Type | Description |
|-------|------|-------------|
| `device_name` | string | Device friendly name |
| `fluidnc_ip` | string | FluidNC controller IP address |
| `temp_low` | number | Low temperature threshold (°C) |
| `temp_high` | number | High temperature threshold (°C) |
| `fan_min` | number | Minimum fan speed (%) |
| `psu_low` | number | PSU low voltage alert (V) |
| `psu_high` | number | PSU high voltage alert (V) |
| `graph_time` | number | Graph timespan (seconds) |
| `graph_interval` | number | Graph update interval (seconds) |

### Status JSON

**Endpoint**: `/api/status`
**Location**: `src/main.cpp:1315-1336`

```json
{
  "machine_state": "IDLE",
  "temperatures": [25.50, 27.30, 24.80, 26.10],
  "fan_speed": 45,
  "fan_rpm": 2340,
  "psu_voltage": 12.15,
  "wpos_x": 10.500,
  "wpos_y": 20.750,
  "wpos_z": -5.000,
  "mpos_x": 110.500,
  "mpos_y": 120.750,
  "mpos_z": 45.000,
  "connected": true
}
```

**Fields**:
| Field | Type | Units | Description |
|-------|------|-------|-------------|
| `machine_state` | string | - | Current CNC machine state |
| `temperatures` | array[4] | °C | Temperature sensors [0-3] |
| `fan_speed` | number | % | Fan speed percentage |
| `fan_rpm` | number | RPM | Fan speed in RPM |
| `psu_voltage` | number | V | PSU voltage (2 decimals) |
| `wpos_x` | number | mm | Work X position (3 decimals) |
| `wpos_y` | number | mm | Work Y position (3 decimals) |
| `wpos_z` | number | mm | Work Z position (3 decimals) |
| `mpos_x` | number | mm | Machine X position (3 decimals) |
| `mpos_y` | number | mm | Machine Y position (3 decimals) |
| `mpos_z` | number | mm | Machine Z position (3 decimals) |
| `connected` | boolean | - | FluidNC connection status |

**Update Frequency**: Real-time (call as needed, typically 100-500ms for dashboards)

---

## Configuration Variables

Configuration stored in `Preferences` (EEPROM emulation) and accessed via `cfg` struct.

**Location**: `src/config/config.h` and `src/config/config.cpp`

### Display Configuration

| Variable                   | Type     | Default     | Description                         |
| -------------------------- | -------- | ----------- | ----------------------------------- |
| `cfg.device_name`          | char[32] | "FluidDash" | Device friendly name                |
| `cfg.use_fahrenheit`       | bool     | false       | Temperature in °F instead of °C     |
| `cfg.use_inches`           | bool     | false       | Coordinates in inches instead of mm |
| `cfg.coord_decimal_places` | uint8_t  | 3           | Position decimal places (0-4)       |

### Temperature & Fan

| Variable                  | Type    | Default | Range | Description              |
| ------------------------- | ------- | ------- | ----- | ------------------------ |
| `cfg.temp_threshold_low`  | float   | 30.0    | 0-100 | Low temp threshold (°C)  |
| `cfg.temp_threshold_high` | float   | 50.0    | 0-100 | High temp threshold (°C) |
| `cfg.fan_min_speed`       | uint8_t | 20      | 0-100 | Minimum fan speed (%)    |
| `cfg.fan_max_speed_limit` | uint8_t | 100     | 0-100 | Maximum fan speed (%)    |

**Fan Control Logic**:

```cpp
if (temp < temp_threshold_low) {
    fanSpeed = fan_min_speed;
} else if (temp > temp_threshold_high) {
    fanSpeed = fan_max_speed_limit;
} else {
    fanSpeed = map(temp, temp_threshold_low, temp_threshold_high,
                   fan_min_speed, fan_max_speed_limit);
}
```

### PSU Monitoring

| Variable              | Type  | Default | Description                      |
| --------------------- | ----- | ------- | -------------------------------- |
| `cfg.psu_alert_low`   | float | 11.0    | Low voltage alert threshold (V)  |
| `cfg.psu_alert_high`  | float | 13.0    | High voltage alert threshold (V) |
| `cfg.psu_voltage_cal` | float | 5.0     | Voltage calibration multiplier   |

**Voltage Calculation**:

```cpp
measuredVoltage = (adcValue / ADC_RESOLUTION) * 3.3;
psuVoltage = measuredVoltage * cfg.psu_voltage_cal;
```

### Graph Configuration

| Variable                     | Type     | Default | Range   | Description                               |
| ---------------------------- | -------- | ------- | ------- | ----------------------------------------- |
| `cfg.graph_timespan_seconds` | uint16_t | 3600    | 60-7200 | Graph history duration (1 min to 2 hours) |
| `cfg.graph_update_interval`  | uint8_t  | 5       | 1-60    | Graph update interval (seconds)           |

**Memory Impact**:

- Buffer size = `graph_timespan_seconds / graph_update_interval`
- Default: 3600s / 5s = 720 points × 4 bytes = 2880 bytes RAM

### Calibration Offsets

| Variable             | Type  | Default | Range      | Description               |
| -------------------- | ----- | ------- | ---------- | ------------------------- |
| `cfg.temp_offset_x`  | float | 0.0     | -50 to +50 | Temp sensor 0 offset (°C) |
| `cfg.temp_offset_yl` | float | 0.0     | -50 to +50 | Temp sensor 1 offset (°C) |
| `cfg.temp_offset_yr` | float | 0.0     | -50 to +50 | Temp sensor 2 offset (°C) |
| `cfg.temp_offset_z`  | float | 0.0     | -50 to +50 | Temp sensor 3 offset (°C) |

### Network Configuration

| Variable           | Type     | Default         | Description            |
| ------------------ | -------- | --------------- | ---------------------- |
| `cfg.fluidnc_ip`   | char[32] | "192.168.1.100" | FluidNC WebSocket IP   |
| `cfg.fluidnc_port` | uint16_t | 80              | FluidNC WebSocket port |

---

## Usage Examples

### Example 1: Read Status via API

**JavaScript (Web Dashboard)**:

```javascript
async function updateStatus() {
    const response = await fetch('/api/status');
    const data = await response.json();

    document.getElementById('temp0').innerText = data.temperatures[0].toFixed(1) + '°C';
    document.getElementById('machine_state').innerText = data.machine_state;
    document.getElementById('psu_voltage').innerText = data.psu_voltage.toFixed(2) + 'V';
}

setInterval(updateStatus, 500); // Update every 500ms
```

**cURL (Command Line)**:

```bash
curl http://192.168.1.50/api/status
```

**Python**:

```python
import requests

response = requests.get('http://192.168.1.50/api/status')
data = response.json()

print(f"Machine State: {data['machine_state']}")
print(f"Temperature 0: {data['temperatures'][0]:.1f}°C")
print(f"PSU Voltage: {data['psu_voltage']:.2f}V")
```

### Example 2: Update Settings via API

**JavaScript (Settings Form)**:

```javascript
async function saveSettings() {
    const formData = new FormData();
    formData.append('temp_low', document.getElementById('temp_low').value);
    formData.append('temp_high', document.getElementById('temp_high').value);
    formData.append('fan_min', document.getElementById('fan_min').value);

    const response = await fetch('/api/save', {
        method: 'POST',
        body: formData
    });

    const result = await response.text();
    alert(result); // "Settings saved successfully"
}
```

**cURL**:

```bash
curl -X POST http://192.168.1.50/api/save \
  -d "temp_low=35.0" \
  -d "temp_high=55.0" \
  -d "fan_min=25"
```

### Example 3: Custom JSON Screen Layout

**File**: `/monitor.json` on SD card

```json
{
  "name": "Custom Monitor",
  "background": "#000000",
  "elements": [
    {
      "type": "text",
      "x": 10,
      "y": 10,
      "label": "Motor Temperatures",
      "textSize": 2,
      "color": "#00BFFF"
    },
    {
      "type": "temp",
      "x": 10,
      "y": 40,
      "dataSource": "temp0",
      "label": "X-Axis: ",
      "textSize": 3,
      "color": "#00FF00",
      "showLabel": true
    },
    {
      "type": "coord",
      "x": 10,
      "y": 80,
      "dataSource": "wposX",
      "label": "X: ",
      "textSize": 2,
      "color": "#FFFFFF",
      "showLabel": true,
      "decimals": 3
    },
    {
      "type": "status",
      "x": 10,
      "y": 120,
      "dataSource": "machineState",
      "textSize": 3
    },
    {
      "type": "rect",
      "x": 0,
      "y": 160,
      "w": 480,
      "h": 2,
      "color": "#444444",
      "filled": true
    }
  ]
}
```

**Load & Display**:

1. Upload JSON to SD card via `/upload` page
2. Device automatically loads on boot or via `/api/reload-screens`
3. Press button to cycle through Monitor/Position/Graph modes

### Example 4: Home Assistant Integration

**YAML Configuration**:

```yaml
sensor:
  - platform: rest
    name: FluidDash Temperature
    resource: http://192.168.1.50/api/status
    value_template: '{{ value_json.temperatures[0] }}'
    unit_of_measurement: '°C'
    scan_interval: 10

  - platform: rest
    name: FluidDash PSU Voltage
    resource: http://192.168.1.50/api/status
    value_template: '{{ value_json.psu_voltage }}'
    unit_of_measurement: 'V'
    scan_interval: 10

binary_sensor:
  - platform: rest
    name: FluidDash FluidNC Connected
    resource: http://192.168.1.50/api/status
    value_template: '{{ value_json.connected }}'
    device_class: connectivity
```

---

## Notes & Best Practices

### Performance Considerations

1. **API Polling Rate**:
   
   - Recommended: 500ms - 1s for web dashboards
   - Maximum: 100ms (not recommended, high CPU load)
   - `/api/status` is lightweight (~200 bytes JSON)

2. **Memory Usage**:
   
   - Temperature history buffer dynamically allocated
   - Large `graph_timespan_seconds` values consume more RAM
   - Max recommended: 7200 seconds (2 hours) = ~5.7KB

3. **WebSocket vs REST**:
   
   - FluidNC uses WebSocket for real-time CNC updates
   - Web API uses REST for simpler integration
   - Consider WebSocket for <100ms latency requirements

### Data Precision

- **Temperatures**: DS18B20 provides ±0.5°C accuracy, 0.0625°C resolution
- **PSU Voltage**: ADC 12-bit (0.0008V resolution), accuracy depends on calibration
- **Positions**: Displayed precision configurable (0-4 decimals), typically 3 decimals (µm)
- **Fan RPM**: Calculated from 2 pulses/revolution, accuracy ±5 RPM

### Future Enhancements

#### Planned Data Variables (Phase 2-7)

- **Sensor Mappings**: `sensorMappings[]` for friendly sensor names
- **Sensor UIDs**: `getDiscoveredUIDs()` for DS18B20 identification
- **Job Progress**: `jobProgressPercent`, `jobTimeRemaining`
- **Extended Position**: A-axis support in API responses

#### Planned API Endpoints

- `/api/sensors` - GET sensor list with UIDs and names
- `/api/sensors/discover` - GET discovered DS18B20 sensors
- `/api/sensors/identify` - POST touch-based sensor ID
- `/api/sensors/config` - GET/POST sensor configuration
- `/api/job/start`, `/api/job/stop` - Job control

---

## Troubleshooting

### Variable Shows 0.0 or Default Value

**Possible Causes**:

1. Sensor not connected (temperatures, PSU voltage)
2. FluidNC not connected (positions, machine state)
3. Variable not yet initialized (occurs during boot)

**Debug Steps**:

- Check serial monitor for `[SENSORS]` initialization messages
- Verify WebSocket connection to FluidNC: `fluidncConnected` should be `true`
- Call `/api/status` to verify data is being collected

### API Returns Old/Stale Data

**Possible Causes**:

1. Variables updated in `loop()`, ensure main loop running
2. ADC sampling not complete (`adcReady` flag)
3. Watchdog timeout blocking execution

**Solution**: Variables update continuously in main loop. If data stale, check for blocking delays or watchdog resets.

### JSON Layout Not Displaying Correct Data

**Possible Causes**:

1. Incorrect `dataSource` string (case-sensitive!)
2. Element `type` doesn't match data type (e.g., using `"text"` instead of `"temp"`)
3. JSON parsing error (check serial monitor for `[JSON]` error messages)

**Debug Steps**:

- Verify data source string matches table above exactly
- Check `/api/status` shows expected data
- Review serial monitor: `[JSON] Loading screen config: ...`

---

## Related Documentation

- **Hardware Pins**: See `src/config/pins.h` for GPIO assignments
- **Configuration**: See `src/config/config.h` for all config variables
- **Display System**: See `src/display/screen_renderer.cpp` for JSON rendering
- **Sensor Implementation**: See `DS18B20_PHASE1_IMPLEMENTATION.md` for temperature details
- **Network Functions**: See `src/network/network.cpp` for FluidNC WebSocket

---

**Document Version**: 1.0
**Last Updated**: 2025-11-03
**Firmware Version**: FluidDash v0.1 (CYD Edition)
**Compatibility**: ESP32-2432S028 (CYD 3.5" & 4.0")
