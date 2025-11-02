# FluidDash Project

## Project Overview

**FluidDash** is an ESP32-based monitoring and control system for CNC machines running FluidNC firmware. It provides real-time temperature monitoring, fan control, PSU voltage monitoring, and machine status display through a 480x320 TFT touchscreen interface.

**Version:** v0.7  
**Hardware Platform:** ESP32 DevKit  
**Primary Use Case:** CNC machine thermal management and status monitoring

### Key Features

- **4-channel temperature monitoring** (X, YL, YR, Z motor drivers)
- **Automatic fan control** based on temperature thresholds
- **PSU voltage monitoring** with alerts
- **FluidNC integration** via WebSocket for real-time machine status
- **Web interface** for configuration and remote monitoring
- **WiFi connectivity** with captive portal setup
- **RTC timekeeping** for session tracking
- **Temperature history graphing** with configurable timespan

---

## Hardware Specifications

### Display
- **Model:** ST7796 TFT LCD
- **Resolution:** 480x320 pixels
- **Color Depth:** 16-bit (RGB565)
- **Interface:** SPI
- **Orientation:** Landscape (rotation = 1, 90°)

### Pin Configuration
```
TFT_CS    = 15
TFT_DC    = 2
TFT_RST   = 4
TFT_MOSI  = 23
TFT_SCK   = 18
FAN_PWM   = 25
FAN_TACH  = 26
THERM_X   = 32  (ADC1_CH4)
THERM_YL  = 33  (ADC1_CH5)
THERM_YR  = 34  (ADC1_CH6)
THERM_Z   = 35  (ADC1_CH7)
PSU_VOLT  = 36  (ADC1_CH0)
BTN_MODE  = 14
I2C_SDA   = 21  (RTC)
I2C_SCL   = 22  (RTC)
```

### Sensors
- **Thermistors:** 4x 10kΩ NTC thermistors (B=3950)
- **RTC:** DS3231 Real-Time Clock
- **Fan:** 4-pin PWM fan with tachometer
- **PSU Monitor:** Voltage divider (calibration factor ~7.3x)

---

## Tech Stack

### Platform
- **MCU:** ESP32 (dual-core, 240MHz)
- **Framework:** Arduino (ESP32 Core)
- **Build System:** PlatformIO
- **IDE:** VS Code with PlatformIO plugin

### Libraries & Dependencies
```ini
# Core Libraries
Arduino Framework (espressif32@^6.8.0)
Arduino_GFX_Library@1.3.7          # Display driver
lvgl@^8.3.11                        # UI framework (LVGL 8.3)
RTClib@^2.1.4                       # RTC support
WiFi (built-in)                     # WiFi connectivity
Preferences (built-in)              # NVS storage

# Networking
ESPAsyncWebServer                   # Async web server
AsyncTCP                            # Async TCP support
WiFiManager                         # WiFi configuration
WebSocketsClient@2.7.1              # FluidNC WebSocket

# Optional
WebSocketsClient (stub if not installed)
```

### UI Development
- **LVGL:** Version 8.3.11 (Light and Versatile Graphics Library)
- **SquareLine Studio:** Visual UI designer for LVGL
- **Export Format:** C files (ui.c, ui.h, screen files)

---

## Architecture

### Display System

**Display Modes:**
1. **MODE_MONITOR** (0) - Main dashboard with temps, status, coordinates
2. **MODE_ALIGNMENT** (1) - Large coordinate display for machine setup
3. **MODE_GRAPH** (2) - Full-screen temperature history graph
4. **MODE_NETWORK** (3) - WiFi and FluidNC connection status

**Display Stack:**
```
User Input → Button Handler → Mode Switch
                ↓
         LVGL Screen Load
                ↓
    UI Update Functions (1Hz)
                ↓
         LVGL Timer Handler
                ↓
      Display Driver (flush_cb)
                ↓
        Arduino_GFX → SPI → TFT
```

### Sensor System

**Non-Blocking ADC Sampling:**
- Samples one sensor every 5ms (200 samples/sec total)
- 10 samples per sensor for averaging
- Complete cycle every 250ms
- Processing triggered when all sensors sampled

**Data Flow:**
```
ADC Pin → Sample Buffer (10 samples) → Average → 
Thermistor Calculation → Temperature + Offset → 
Fan Control + History + Display
```

### Network Architecture

**WiFi Modes:**
1. **Station Mode (STA):** Connects to existing WiFi network
2. **AP Mode:** Creates "FluidDash-Setup" access point for configuration
3. **Standalone Mode:** Continues monitoring without WiFi

**Web Server:**
- Port 80 (HTTP)
- Async server for non-blocking operation
- Endpoints: `/`, `/settings`, `/admin`, `/wifi`, `/api/*`
- mDNS: `fluiddash.local` (if WiFi connected)

**FluidNC Connection:**
- WebSocket client to FluidNC on port 81
- Auto-reconnect every 5 seconds if disconnected
- Status updates at configurable rate (default 200ms)
- Parses machine state, coordinates, feed rate, spindle RPM

### Configuration System

**Storage:** ESP32 NVS (Non-Volatile Storage) via Preferences library  
**Namespace:** "fluiddash"  
**Persistence:** Survives power cycles and firmware updates

**Configuration Categories:**
- Network settings (device name, FluidNC IP/port)
- Temperature thresholds and calibration offsets
- Fan control parameters
- PSU monitoring settings
- Display preferences
- Graph settings
- Units and advanced options

---

## File Structure

```
fluiddash/
├── src/
│   ├── main.cpp                    # Main application logic
│   ├── lvgl_driver.cpp             # LVGL display driver adapter
│   ├── ui_helpers.cpp              # UI update helper functions
│   └── ui/                         # SquareLine Studio exports
│       ├── ui.h                    # Main UI header
│       ├── ui.c                    # UI initialization
│       ├── ui_ScreenMonitor.c      # Monitor mode UI
│       ├── ui_ScreenAlignment.c    # Alignment mode UI
│       ├── ui_ScreenGraph.c        # Graph mode UI
│       ├── ui_ScreenNetwork.c      # Network status UI
│       └── ui_events.c             # Event handlers
├── include/
│   ├── lv_conf.h                   # LVGL configuration
│   ├── lvgl_driver.h               # Display driver header
│   └── ui_helpers.h                # Helper functions header
├── platformio.ini                  # PlatformIO configuration
├── CLAUDE.md                       # This file - project context
└── SquareLine_Studio_Integration_Guide.md
```

### Key Files Explained

**main.cpp** (2000+ lines)
- Application entry point
- setup() and loop() functions
- All core logic: sensors, fan control, networking, display
- Web server endpoints and HTML generation
- FluidNC WebSocket handling
- Configuration management

**lvgl_driver.cpp**
- Bridges Arduino_GFX and LVGL
- Display flush callback
- Double-buffered rendering (480x20 lines x2)
- Memory: ~38KB RAM for buffers

**ui_helpers.cpp**
- Wrapper functions to update LVGL widgets
- Separates UI updates from business logic
- Functions like `ui_update_temperatures()`, `ui_update_status()`
- Makes it easy to update UI from anywhere in code

**lv_conf.h**
- LVGL library configuration
- Memory allocation (48KB default)
- Enabled fonts, widgets, features
- Performance settings
- **Important:** Only enable fonts/widgets you actually use to save memory

---

## Coding Standards

### Memory Management

**Critical:** ESP32 has limited RAM. Always be memory-conscious.

- **Display buffers:** 480x20 pixels x2 = ~38KB (can reduce if needed)
- **LVGL memory:** 48KB default (adjustable in lv_conf.h)
- **Temperature history:** Dynamic allocation based on graph settings
  - Example: 300sec / 5sec interval = 60 points = 240 bytes
  - Max capped at 2000 points (8KB) to prevent OOM
- **HTML strings:** Large but temporary, generated on-demand
- **Avoid:** Long-lived String objects, use char arrays where possible

### Non-Blocking Code

**Rule:** Never use `delay()` in production code (except brief waits during setup)

**Correct Pattern:**
```cpp
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000; // 1 second

void loop() {
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    doSomething();
    lastUpdate = millis();
  }
}
```

**ADC Sampling Example:**
- Samples every 5ms without blocking
- State machine: advance through sensors and sample indices
- Sets `adcReady` flag when complete cycle done

### Watchdog Timer

**Timeout:** 10 seconds  
**Critical:** Must call `feedLoopWDT()` regularly to prevent resets

**Rules:**
1. Call at start of `loop()`
2. Call before/after any long operation (>1 second)
3. Call during `setup()` between major initializations
4. **Never** have any function that runs >5 seconds without feeding WDT

### Temperature Calculations

**Steinhart-Hart Equation for NTC Thermistors:**
```cpp
// Constants
#define SERIES_RESISTOR 10000      // 10kΩ pull-up
#define THERMISTOR_NOMINAL 10000   // 10kΩ @ 25°C
#define TEMPERATURE_NOMINAL 25     // 25°C
#define B_COEFFICIENT 3950         // Beta value
#define ADC_RESOLUTION 4095.0      // 12-bit ADC

// Process: ADC → Voltage → Resistance → Temperature → + Offset
```

**Calibration Offsets:**
- Stored per sensor (X, YL, YR, Z)
- Applied after temperature calculation
- Adjustable via admin web interface
- Typically ±2°C for calibration

### Fan Control

**Algorithm:** Linear interpolation between thresholds
```
temp < low_threshold:  fan = min_speed
temp > high_threshold: fan = max_speed_limit
between:               fan = linear mapping
```

**PWM:**
- Frequency: 25kHz
- Resolution: 8-bit (0-255)
- Channel: 0
- Pin: 25

**Tachometer:**
- Interrupt on falling edge
- RPM = (pulses * 60) / 2  (2 pulses per revolution)
- Calculated every 1 second

### Color Definitions

```cpp
#define COLOR_BG       0x0000  // Black
#define COLOR_HEADER   0x001F  // Dark Blue
#define COLOR_TEXT     0xFFFF  // White
#define COLOR_VALUE    0x07FF  // Cyan
#define COLOR_WARN     0xF800  // Red
#define COLOR_GOOD     0x07E0  // Green
#define COLOR_LINE     0x4208  // Gray
#define COLOR_ORANGE   0xFD20  // Orange
```

**LVGL Color Conversion:**
```cpp
lv_color_hex(0x07FF)  // Cyan in LVGL
```

### String Formatting

**For display updates:**
```cpp
// Temperatures (no decimal for display)
sprintf(buffer, "%dC", (int)temperature);

// Coordinates (configurable decimals)
if (decimals == 3) {
  sprintf(buffer, "X:%7.3f", coordinate);
} else {
  sprintf(buffer, "X:%6.2f", coordinate);
}

// PSU Voltage
sprintf(buffer, "%.1fV", voltage);
```

---

## Development Workflow

### Initial Setup

1. **Clone/Open Project** in VS Code with PlatformIO
2. **Verify platformio.ini** settings (upload port, libraries)
3. **Create/Check CLAUDE.md** in project root (this file)
4. **Install LVGL** (automatic via PlatformIO lib_deps)
5. **Configure SquareLine Studio** (if using):
   - Project: 480x320, 16-bit, LVGL 8.3
   - Export path: `src/ui/`

### UI Development with SquareLine Studio

**Workflow:**
```
1. Design in SquareLine Studio
   ↓
2. Export UI files → src/ui/
   ↓
3. Update ui_helpers.cpp (if widgets added/renamed)
   ↓
4. Update main.cpp update functions
   ↓
5. Build & Upload
   ↓
6. Test on hardware
   ↓
7. Iterate (back to step 1)
```

**Key Points:**
- Name widgets clearly: `label_temp_x`, `chart_temp_history`
- Use dark theme to match existing UI
- Keep LVGL fonts enabled in lv_conf.h before using in Studio
- Export overwrites files safely - version control recommended

### Build & Upload

**PlatformIO Commands:**
```bash
# Build only
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor

# Clean build
pio run --target clean

# Update libraries
pio pkg update
```

**VS Code Shortcuts:**
- Build: `Ctrl+Alt+B` (checkmark icon)
- Upload: `Ctrl+Alt+U` (arrow icon)
- Monitor: `Ctrl+Alt+S` (plug icon)
- Upload & Monitor: `Ctrl+Alt+T`

### Testing Workflow

1. **Connect ESP32** via USB (check COM port in platformio.ini)
2. **Upload firmware**
3. **Monitor serial output** for errors/status
4. **Test WiFi connection**:
   - Should auto-connect if credentials saved
   - Otherwise, hold button 10sec for AP mode
5. **Test web interface**: http://fluiddash.local or http://[IP]
6. **Verify sensor readings** (temperatures, PSU voltage)
7. **Test FluidNC connection** (if FluidNC available)
8. **Check display modes** (button press to cycle)

### Common Development Tasks

**Add a new sensor:**
1. Define pin in main.cpp
2. Add to ADC sampling loop
3. Add calibration offset to Config struct
4. Update `processAdcReadings()`
5. Add display widget in SquareLine Studio
6. Create helper function in ui_helpers.cpp
7. Update display mode in main.cpp

**Add a new configuration option:**
1. Add field to Config struct
2. Add to `initDefaultConfig()`
3. Add to `loadConfig()` and `saveConfig()`
4. Add input to HTML settings page
5. Add handling in `/api/save` endpoint
6. Use in relevant code section

**Create a new display mode:**
1. Add to DisplayMode enum
2. Design screen in SquareLine Studio → export
3. Add case to `drawScreen()` switch
4. Create `update[ModeName]Mode()` function
5. Add case to `updateDisplay()` switch
6. Add to mode cycling in `cycleDisplayMode()`

**Add a new web endpoint:**
1. Define handler in `setupWebServer()`
2. Implement HTML function if needed (e.g., `getNewPageHTML()`)
3. Add to main HTML navigation if appropriate
4. Handle form submissions in POST handlers
5. Test via browser

---

## Important Constraints & Considerations

### Memory Limitations

**Available RAM:** ~320KB total, but significant portions used by:
- WiFi stack (~40KB)
- LVGL buffers (~38KB)
- LVGL memory pool (~48KB)
- Temperature history (dynamic)
- Web server buffers
- **Usable for application:** ~150KB

**Symptoms of memory issues:**
- Guru Meditation Errors
- Random resets
- WebSocket disconnections
- Display artifacts

**Solutions:**
- Reduce LVGL buffer: `480 * 10` instead of `480 * 20`
- Reduce LVGL memory: `32 * 1024U` instead of `48 * 1024U`
- Limit temperature history size
- Disable unused LVGL fonts/widgets
- Use static strings in PROGMEM for large HTML

### FluidNC WebSocket Protocol

**Connection:**
- URL: `ws://[fluidnc_ip]:[port]/ws`
- Default port: 81
- Auto-reconnect: 5 seconds

**Status Message Format:**
```
<Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000|FS:0,0>
```

**Parsing Rules:**
- Messages start with `<` and end with `>`
- Fields separated by `|`
- State: `Idle`, `Run`, `Hold`, `Alarm`, `Home`, `Jog`
- MPos: Machine position (absolute)
- WPos: Work position (relative to work coordinate system)
- FS: Feed rate, Spindle RPM

**Sending Commands:**
```cpp
webSocket.sendTXT("?\n");  // Request status
webSocket.sendTXT("$X\n"); // Unlock alarm
```

### WiFi Considerations

**Station Mode:**
- Credentials stored in NVS (namespace "fluiddash")
- Keys: "wifi_ssid", "wifi_pass"
- Connection timeout: 10 seconds (20 retries x 500ms)
- mDNS name: `[device_name].local`

**AP Mode:**
- SSID: "FluidDash-Setup"
- IP: 192.168.4.1
- No password (open network)
- Triggered by 10-second button hold
- Web interface accessible at http://192.168.4.1

**Important:** ESP32 cannot scan WiFi networks while in AP mode

### Display Performance

**LVGL Refresh Rate:**
- Default: 30ms (33 FPS)
- Configurable in lv_conf.h: `LV_DISP_DEF_REFR_PERIOD`
- Lower = smoother but more CPU usage

**Update Strategy:**
- Full screen redraw only on mode change
- Partial updates every 1 second
- Only update changed values when possible
- Use `lv_label_set_text()` for text changes (handles redraw)

**Critical:** Always call `lv_timer_handler()` in `loop()` every 5-10ms

### RTC Timekeeping

**DS3231 I2C Address:** 0x68

**Usage:**
```cpp
DateTime now = rtc.now();
int year = now.year();
int month = now.month();
int day = now.day();
int hour = now.hour();
int minute = now.minute();
int second = now.second();
```

**Note:** RTC must be set initially. Not automatically synced with NTP.

---

## Web Interface Details

### HTML Page Structure

All pages generated dynamically as String objects:
- `getMainHTML()` - Dashboard
- `getSettingsHTML()` - User configuration
- `getAdminHTML()` - Calibration settings
- `getWiFiConfigHTML()` - Network setup

**Style:** Dark theme, responsive, mobile-friendly

### API Endpoints

**GET Endpoints:**
```
/                 - Main dashboard
/settings         - User settings page
/admin            - Admin/calibration page
/wifi             - WiFi configuration page
/api/config       - Current config as JSON
/api/status       - Current status as JSON
```

**POST Endpoints:**
```
/api/save         - Save user settings
/api/admin/save   - Save calibration settings
/api/restart      - Restart device
/api/reset-wifi   - Reset WiFi settings
/api/wifi/connect - Connect to WiFi network
```

**JSON Response Examples:**

Config:
```json
{
  "device_name": "fluiddash",
  "fluidnc_ip": "192.168.73.13",
  "temp_low": 30.0,
  "temp_high": 50.0,
  "fan_min": 30,
  "graph_time": 300,
  "graph_interval": 5
}
```

Status:
```json
{
  "machine_state": "RUN",
  "temperatures": [45.2, 42.8, 43.1, 40.5],
  "fan_speed": 65,
  "fan_rpm": 1200,
  "psu_voltage": 24.2,
  "wpos_x": 123.456,
  "wpos_y": 234.567,
  "wpos_z": -5.000,
  "connected": true
}
```

---

## Troubleshooting Guide

### Common Issues

**1. Display Shows Nothing**
- Check SPI wiring (MOSI, SCK, CS, DC, RST)
- Verify display initialized: "Display initialized OK" in serial
- Check `lv_timer_handler()` is called in loop
- Verify LVGL initialization order: GFX → LVGL → UI

**2. ESP32 Keeps Resetting**
- Watchdog timeout - add more `feedLoopWDT()` calls
- Memory overflow - reduce buffer sizes
- Power supply insufficient - use powered USB hub or external 5V

**3. WiFi Won't Connect**
- Check saved credentials in NVS
- Verify router is 2.4GHz (ESP32 doesn't support 5GHz)
- Check WiFi signal strength
- Try manual connection via AP mode
- Clear credentials: delete "wifi_ssid" and "wifi_pass" keys

**4. FluidNC Not Connecting**
- Verify FluidNC IP address (check in web interface or use mDNS scan)
- Check port 81 is open
- Ensure FluidNC WebSocket feature is enabled
- Monitor serial for "[FluidNC] Connected" message
- Try manual IP instead of auto-discovery

**5. Temperatures Reading Zero**
- Check thermistor wiring (should have pull-up resistor)
- Verify ADC pins are correct (must be ADC1 channels)
- Check series resistor value (10kΩ recommended)
- Adjust B coefficient if using different thermistors

**6. Web Interface Slow or Unresponsive**
- HTML pages are large - loading may take 2-3 seconds
- Reduce `UPDATE_INTERVAL` for status updates
- Clear browser cache
- Use local IP instead of mDNS (faster)

**7. Display Updates Laggy**
- Reduce update frequency (increase intervals)
- Simplify screen layout (fewer widgets)
- Increase display buffer size if memory available
- Check CPU usage isn't maxed

**8. Button Not Responding**
- Check pin 14 is connected with pull-up
- Verify button wiring (normally open, pulls to ground)
- Check debounce timing in `handleButton()`

### Debug Serial Output

**Enable verbose logging:**
```cpp
cfg.enable_logging = true;
```

**Key messages to look for:**
```
FluidDash - Starting...
Display initialized OK
Configuration loaded
WiFi Connected!
IP: 192.168.x.x
mDNS started: http://fluiddash.local
[FluidNC] Connected
Web server started
Setup complete - entering main loop
```

**Error indicators:**
```
Display initialization failed
WiFi connection failed
[FluidNC] Disconnected
ERROR: Failed to allocate history buffer
Warning: Buffer size exceeds limit
```

---

## Configuration Reference

### Temperature Settings

**User Configurable:**
- `temp_threshold_low` - When fan starts ramping (default 30°C)
- `temp_threshold_high` - When fan reaches 100% (default 50°C)
- `use_fahrenheit` - Display temperatures in °F

**Admin/Calibration:**
- `temp_offset_x` - Calibration offset for X thermistor (°C)
- `temp_offset_yl` - Calibration offset for YL thermistor (°C)
- `temp_offset_yr` - Calibration offset for YR thermistor (°C)
- `temp_offset_z` - Calibration offset for Z thermistor (°C)

### Fan Settings

- `fan_min_speed` - Minimum fan speed % (default 30)
- `fan_max_speed_limit` - Maximum fan speed % limit (default 100)

### PSU Settings

- `psu_voltage_cal` - Voltage divider multiplier (default 7.3)
- `psu_alert_low` - Low voltage warning (default 23V)
- `psu_alert_high` - High voltage warning (default 25V)

### Display Settings

- `brightness` - Display brightness 0-255 (default 255)
- `default_mode` - Startup display mode (0-3)
- `show_machine_coords` - Show machine coordinates (default true)
- `show_temp_graph` - Show temp graph in monitor mode (default true)
- `coord_decimal_places` - Coordinate precision: 2 or 3 (default 2)

### Graph Settings

- `graph_timespan_seconds` - History duration 60-3600 (default 300 = 5min)
- `graph_update_interval` - Update frequency 1-60 sec (default 5)

### Network Settings

- `device_name` - mDNS hostname (default "fluiddash")
- `fluidnc_ip` - FluidNC IP address (default "192.168.73.13")
- `fluidnc_port` - FluidNC WebSocket port (default 81)
- `fluidnc_auto_discover` - Use mDNS to find FluidNC (default true)

### Advanced Settings

- `enable_logging` - Verbose serial logging (default false)
- `status_update_rate` - FluidNC poll rate in ms (default 200)

---

## Best Practices

### When Making Changes

1. **Test incrementally** - Don't change too many things at once
2. **Use version control** - Git is your friend
3. **Comment your changes** - Future you will thank you
4. **Keep backups** - Save working firmware binaries
5. **Monitor serial output** - Catch errors early
6. **Test edge cases** - Disconnected WiFi, missing FluidNC, extreme temps

### Code Organization

- **Keep functions focused** - Single responsibility principle
- **Use helper functions** - Don't repeat code
- **Group related code** - Use comments to mark sections
- **Name clearly** - `calculateThermistorTemp()` not `calcTemp()`
- **Document complex logic** - Especially hardware-specific stuff

### Performance Tips

- **Cache calculations** - Don't recalculate constants
- **Update only what changed** - Avoid full screen redraws
- **Use appropriate data types** - uint8_t for 0-255, float for temps
- **Profile memory usage** - Check heap free with `ESP.getFreeHeap()`
- **Minimize String usage** - Use char arrays for fixed-length strings

### Security Considerations

- **WiFi credentials** stored in NVS (not visible in code)
- **No authentication** on web interface (consider for production)
- **Open AP mode** (no password) - for initial setup only
- **Local network only** - Not exposed to internet
- **No sensitive data** - Configuration is not secret

---

## Additional Resources

### Documentation Links

- **PlatformIO:** https://docs.platformio.org/
- **ESP32 Arduino Core:** https://docs.espressif.com/projects/arduino-esp32/
- **LVGL Documentation:** https://docs.lvgl.io/8.3/
- **SquareLine Studio:** https://docs.squareline.io/
- **Arduino_GFX:** https://github.com/moononournation/Arduino_GFX
- **FluidNC:** https://github.com/bdring/FluidNC/wiki

### Related Files in Project

- `SquareLine_Studio_Integration_Guide.md` - Detailed LVGL + SquareLine setup
- `platformio.ini` - Build configuration
- `main.cpp` - Full source code with inline comments

---

## Project-Specific Vocabulary

**Terms to understand:**

- **WPos** - Work Position (coordinates relative to work coordinate system)
- **MPos** - Machine Position (absolute coordinates from home)
- **FluidNC** - CNC controller firmware (replaces Grbl on ESP32)
- **Thermistor** - Temperature-sensitive resistor (NTC type)
- **NVS** - Non-Volatile Storage (ESP32's flash memory for config)
- **mDNS** - Multicast DNS (allows .local domain names)
- **Tachometer** - Fan speed sensor (pulses per revolution)
- **Watchdog** - Timer that resets system if not "fed" regularly
- **Context Window** - Amount of information Claude can consider at once

**Hardware terms:**

- **Driver** - Stepper motor driver (gets hot, needs cooling)
- **PSU** - Power Supply Unit (24V for CNC)
- **RTC** - Real-Time Clock (keeps time when powered off)
- **TFT** - Thin-Film Transistor (LCD display type)
- **PWM** - Pulse Width Modulation (for fan speed control)
- **ADC** - Analog-to-Digital Converter (reads voltages)

---

## Quick Start Checklist

For new developers joining the project:

- [ ] Clone project and open in VS Code
- [ ] Install PlatformIO plugin
- [ ] Read this CLAUDE.md file
- [ ] Review main.cpp structure
- [ ] Check platformio.ini settings (upload port)
- [ ] Build project (should succeed)
- [ ] Connect ESP32 via USB
- [ ] Upload firmware
- [ ] Monitor serial output
- [ ] Test button press (cycle modes)
- [ ] Hold button 10 sec (enter WiFi config)
- [ ] Connect to FluidDash-Setup AP
- [ ] Access web interface at 192.168.4.1
- [ ] Configure WiFi credentials
- [ ] Test web interface on local network
- [ ] (Optional) Install SquareLine Studio
- [ ] (Optional) Review UI design in SquareLine

---

## Current Development Status

**Completed Features:**
- ✅ 4-channel temperature monitoring
- ✅ Automatic fan control
- ✅ PSU voltage monitoring
- ✅ FluidNC WebSocket integration
- ✅ Web configuration interface
- ✅ WiFi manager with AP mode
- ✅ RTC timekeeping
- ✅ Temperature history graphing
- ✅ 4 display modes
- ✅ Configuration persistence
- ✅ Button mode switching

**Known Issues:**
- WiFi scanning not available in AP mode (ESP32 limitation)
- Large HTML strings can cause heap fragmentation
- No authentication on web interface
- Temperature graph limited to 2000 points max
- Watchdog timeout if blocking operations exceed 10 seconds

**Planned Enhancements:**
- Touch screen support (if hardware added)
- Alarm notifications (buzzer/LED)
- Data logging to SD card
- MQTT support for remote monitoring
- OTA firmware updates
- Multiple FluidNC machine support
- Custom themes in UI

**SquareLine Studio Integration:**
- ✅ LVGL 8.3.11 framework integrated
- ✅ SquareLine Studio UI designed and exported
- ✅ Monitor screen (`ui_Screen1Monitor`) implemented with:
  - Header with title and real-time datetime
  - Temperature panel with 4 channels (X, YL, YR, Z) and peak values
  - Temperature history chart component (480x72px)
  - FluidNC status panel with WCS/MCS coordinates
  - Fan% and PSU voltage displays
- ✅ Helper functions created to update all LVGL labels
- ⏳ Chart data population (TODO)
- ⏳ Additional screens for Alignment, Graph, Network modes
- See `SquareLine_Studio_Integration_Guide.md` for details

---

## Notes for Claude Code

When working on this project:

1. **Always check memory usage** - ESP32 RAM is limited
2. **Feed the watchdog** - Add `feedLoopWDT()` in long operations
3. **Use non-blocking code** - No delays in loop()
4. **Test on hardware** - Simulator can't catch everything
5. **Update this file** - Keep CLAUDE.md current as project evolves
6. **Reference the integration guide** - When working with LVGL/SquareLine
7. **Check serial monitor** - First place to look for errors
8. **Respect pin assignments** - Hardware is soldered, can't easily change
9. **Document assumptions** - Especially hardware-specific calculations
10. **Ask before major changes** - Especially to memory layout or architecture

**When suggesting changes:**
- Consider memory impact
- Maintain non-blocking patterns
- Keep watchdog in mind
- Test incrementally
- Provide rollback instructions
- Update documentation if needed

---

**Last Updated:** 2025-11-01
**Project Version:** v0.8 (LVGL Migration)
**Author:** FluidDash Development Team
**Status:** Active Development

---

## Recent Changes (v0.8)

**2025-11-01 - LVGL/SquareLine Studio Integration:**

1. **UI Framework Migration**
   - Migrated from direct Arduino_GFX drawing to LVGL 8.3.11
   - Integrated SquareLine Studio for visual UI design
   - Created comprehensive Monitor screen layout

2. **Screen Implementation** (`ui_Screen1Monitor`)
   - Header panel (480x25): Title + real-time datetime
   - Temperature panel (180x125): 4-channel monitoring with peak tracking
   - Graph panel (300x125): Temperature history chart component
   - FluidNC panel (480x170): Status, WCS, and MCS coordinates display
   - Fan/PSU status line

3. **Code Updates**
   - Updated `main.cpp` to use `ui_Screen1Monitor` instead of old screen refs
   - Created comprehensive `updateMonitorMode()` with all label updates:
     - DateTime, temperatures (X/YL/YR/Z), peak temps
     - Fan speed/RPM, PSU voltage
     - FluidNC connection status
     - Work and Machine coordinate systems
   - Fixed screen background color (black instead of blue)
   - Enabled `lv_font_montserrat_10` in `lv_conf.h`

4. **Memory Optimizations**
   - Display buffers: 480x10 lines (reduced from 20)
   - LVGL memory pool: 32KB (reduced from 48KB)
   - Total RAM saved: ~35KB

5. **Files Modified**
   - `src/main.cpp` - Screen references and update functions
   - `src/ui/ui_Screen1Monitor.c` - Background color fix
   - `include/lv_conf.h` - Font enable
   - `platformio.ini` - LVGL library added

6. **Known Issues Resolved**
   - Fixed variable name mismatch (`peakTemps` vs `peakTemperatures`)
   - Fixed missing font compilation error
   - Removed old Arduino_GFX overlay drawing code

**Next Steps:**
- Test new UI on hardware
- Implement chart data population
- Create additional screens (Alignment, Graph, Network)
- Consider LovyanGFX migration for FluidNC ecosystem compatibility
