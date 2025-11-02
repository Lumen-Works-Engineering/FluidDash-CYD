# Claude Code Task: Convert FluidDash to CYD Hardware

## Objective
Convert the FluidDash main.cpp file from generic ESP32 pin configuration to CYD (ESP32-2432S028) hardware pins.

## Context
- Current file: `main_cpp.txt` (2228 lines) has generic ESP32 pins
- Target hardware: CYD 3.5" or 4.0" (E32R35T/E32R40T)
- Reference: See `FLUIDDASH_HARDWARE_REFERENCE.md` for complete CYD pin specifications
- This is a pin mapping change only - all functionality remains the same

## Task List

### Task 1: Setup Project Files
**Action:** 
1. Copy `main_cpp.txt` to `src/main.cpp`
2. Copy `platformio_ini.txt` to `platformio.ini` (root directory)
3. Verify both files exist before proceeding

### Task 2: Update Version Header
**File:** `src/main.cpp`
**Location:** Lines 1-8

**Change the comment block from:**
```cpp
/*
 * FluidDash v0.08
 * - WiFiManager for initial setup
```

**To:**
```cpp
/*
 * FluidDash v0.09 - CYD Edition
 * Configured for ESP32-2432S028 (CYD 3.5" or 4.0" modules)
 * - WiFiManager for initial setup
```

### Task 3: Replace Pin Definitions (CRITICAL)
**File:** `src/main.cpp`
**Location:** Around lines 48-62 (section starting with `// Hardware pins`)

**Replace the entire pin definition block with:**

```cpp
// ========== CYD HARDWARE PIN CONFIGURATION ==========
// Compatible with E32R35T (3.5") and E32R40T (4.0")
// Based on LCD Wiki official documentation - see FLUIDDASH_HARDWARE_REFERENCE.md

// Display & Touch (Pre-wired onboard - HSPI bus)
#define TFT_CS      15    // LCD chip select
#define TFT_DC      2     // Data/command
#define TFT_RST     -1    // Shared with EN button (hardware reset)
#define TFT_MOSI    13    // SPI MOSI (HSPI)
#define TFT_SCK     14    // SPI clock (HSPI)
#define TFT_MISO    12    // SPI MISO (HSPI)
#define TFT_BL      27    // Backlight control
#define TOUCH_CS    33    // Touch chip select
#define TOUCH_IRQ   36    // Touch interrupt

// FluidDash Sensors (External connections via connectors)
#define ONE_WIRE_BUS_1    21    // Internal motor drivers (P3 SPI_CS pin)
#define RTC_SDA           32    // I2C connector (P4)
#define RTC_SCL           25    // I2C connector (P4)
#define FAN_PWM           4     // Fan PWM control (repurpose AUDIO_EN)
#define FAN_TACH          35    // Fan tachometer (P2 expansion pin)
#define PSU_VOLT          34    // PSU voltage monitor (repurpose BAT_ADC)

// RGB Status LED (Pre-wired onboard - common anode, LOW=on)
#define LED_RED     22
#define LED_GREEN   16
#define LED_BLUE    17

// Mode button (use GPIO0 - BOOT button)
#define BTN_MODE    0
```

**Note:** This removes the old thermistor pins (THERM_X, THERM_YL, THERM_YR, THERM_Z) and replaces with ONE_WIRE_BUS_1 for digital DS18B20 sensors.

### Task 4: Update LGFX Class - Add Backlight Instance
**File:** `src/main.cpp`
**Location:** Around line 172 (inside `class LGFX`)

**After this line:**
```cpp
lgfx::Bus_SPI _bus_instance;
```

**Add:**
```cpp
lgfx::Light_PWM _light_instance;
```

### Task 5: Update LGFX Class - Change SPI Host
**File:** `src/main.cpp`
**Location:** Around line 180 (inside LGFX constructor)

**Find:**
```cpp
cfg.spi_host = VSPI_HOST;
```

**Replace with:**
```cpp
cfg.spi_host = HSPI_HOST;      // CRITICAL: CYD uses HSPI not VSPI!
```

### Task 6: Update LGFX Class - Fix Panel Settings
**File:** `src/main.cpp`
**Location:** Around lines 210-213 (inside LGFX constructor, panel config section)

**Find these three lines:**
```cpp
cfg.invert           = false;
cfg.rgb_order        = false;
cfg.bus_shared       = false;
```

**Replace with:**
```cpp
cfg.invert           = true;        // CYD needs inversion
cfg.rgb_order        = true;        // CYD uses BGR
cfg.bus_shared       = true;        // Shared with touch
```

### Task 7: Update LGFX Class - Add Backlight Configuration
**File:** `src/main.cpp`
**Location:** Around line 217 (inside LGFX constructor, just BEFORE `setPanel(&_panel_instance);`)

**Add this configuration block:**
```cpp
{
  auto cfg = _light_instance.config();
  cfg.pin_bl = TFT_BL;      // GPIO27
  cfg.invert = false;
  cfg.freq   = 44100;
  cfg.pwm_channel = 1;
  _light_instance.config(cfg);
  _panel_instance.setLight(&_light_instance);
}
```

### Task 8: Update setup() - I2C Initialization
**File:** `src/main.cpp`
**Location:** Around line 350 (inside `setup()` function)

**Find:**
```cpp
Wire.begin();
```

**Replace with:**
```cpp
Wire.begin(RTC_SDA, RTC_SCL);  // CYD I2C pins: GPIO32=SDA, GPIO25=SCL
```

### Task 9: Update setup() - Add RGB LED Initialization
**File:** `src/main.cpp`
**Location:** Around line 360 (in `setup()`, after other pinMode calls but before display init)

**Add this block:**
```cpp
// RGB LED setup (common anode - LOW=on)
pinMode(LED_RED, OUTPUT);
pinMode(LED_GREEN, OUTPUT);
pinMode(LED_BLUE, OUTPUT);
digitalWrite(LED_RED, HIGH);    // OFF
digitalWrite(LED_GREEN, HIGH);  // OFF
digitalWrite(LED_BLUE, HIGH);   // OFF
```

### Task 10: Verify platformio.ini (Optional Check)
**File:** `platformio.ini`
**Action:** Verify these settings exist (should already be correct):

```ini
upload_port = COM7
monitor_port = COM7
lib_deps =
    lovyan03/LovyanGFX@^1.1.16
```

**If needed:** Update COM port to match your system (check Device Manager on Windows)

## Verification Steps

After making all changes, verify:

1. **Compilation Check:**
   - Run: `pio run`
   - Should compile without errors
   - Look for "SUCCESS" message

2. **Key Changes Verification:**
   ```bash
   # Verify HSPI_HOST (not VSPI_HOST)
   grep -n "HSPI_HOST" src/main.cpp
   
   # Verify CYD pins
   grep -n "TFT_MOSI.*13" src/main.cpp
   grep -n "TFT_SCK.*14" src/main.cpp
   
   # Verify I2C pins
   grep -n "Wire.begin(RTC_SDA, RTC_SCL)" src/main.cpp
   ```

3. **Upload and Test:**
   - Connect CYD via USB
   - Run: `pio run --target upload`
   - Monitor serial: `pio device monitor`
   - Should see:
     ```
     FluidDash CYD - Starting...
     Watchdog timer enabled
     Initializing display...
     Display initialized
     RTC initialized
     ```

## Critical Success Criteria

✅ **Must haves:**
- SPI host is HSPI_HOST (line ~180)
- TFT_MOSI is 13 (not 23)
- TFT_SCK is 14 (not 18)
- cfg.invert = true (line ~210)
- Wire.begin() has two arguments (line ~350)
- Backlight PWM instance added and configured

❌ **Common mistakes to avoid:**
- Don't change library versions in platformio.ini
- Don't modify any web server code
- Don't change any FluidNC WebSocket code
- Don't modify drawing functions (they're hardware-independent)

## Reference Files

- **Complete pin reference:** `FLUIDDASH_HARDWARE_REFERENCE.md` (in project)
- **Wiring guide:** `FLUIDDASH_WIRING_GUIDE.md` (in project)
- **General project info:** `CLAUDE.md` (in project)

## Notes for Claude Code

- This is a straightforward pin remapping task
- All functionality remains identical - only hardware pins change
- The code structure and logic do NOT change
- If you encounter any ambiguity, refer to FLUIDDASH_HARDWARE_REFERENCE.md
- Original file has 2228 lines - modified file should have similar length
- Main changes are in first ~400 lines (pin definitions and LGFX class)

## Expected Outcome

After completion:
- `src/main.cpp` configured for CYD hardware
- Ready to compile and upload to ESP32-2432S028
- All FluidDash features work on CYD display
- Can connect external sensors per wiring guide

---

**Status:** Ready for Claude Code execution
**Estimated time:** ~2 minutes
**Risk level:** Low (pin mapping only, well-documented)
