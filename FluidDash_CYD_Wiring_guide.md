# FluidDash Wiring Guide
## ESP32-32E Display Module (CYD) External Connections

---

## Overview

The CYD board has most components built-in. Only **external sensors** need wiring:
- DS18B20 temperature sensors (2 buses)
- DS3231 RTC module
- Fan PWM + tachometer
- PSU voltage divider

---

## Connection Diagram (ASCII Art)

```
╔═══════════════════════════════════════════════════════════════════════════╗
║                      ESP32-32E Display Module (Top View)                  ║
║                                                                           ║
║   ┌─────────────────────────────────────────────────────────┐            ║
║   │                  480x320 LCD Display                    │            ║
║   │                                                         │            ║
║   │                                                         │            ║
║   └─────────────────────────────────────────────────────────┘            ║
║                                                                           ║
║  [P4-I2C]  [P3-SPI]  [P2-EXP]    [RGB LED]    [TYPE-C]    [SD CARD]     ║
║     ↓         ↓         ↓           ↓            ↓            ↓          ║
║  RTC Only  Temp Bus1  Fan Tach   Built-in     Power      Built-in       ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

---

## 1. DS3231 RTC Module

**Connector:** P4 (I2C Peripheral - 6-pin 1.25mm JST)

```
DS3231 RTC          P4 Connector (I2C)
┌─────────────┐     ┌───────────────┐
│ VCC   ──────┼─────┤ Pin 1 (VCC)   │ → 3.3V
│ GND   ──────┼─────┤ Pin 2 (GND)   │ → Ground
│ SDA   ──────┼─────┤ Pin 4 (GPIO32)│ → I2C Data
│ SCL   ──────┼─────┤ Pin 5 (GPIO25)│ → I2C Clock
└─────────────┘     └───────────────┘
                    Pin 3, 6: Not connected
```

**Notes:**
- RTC module has built-in pull-up resistors (4.7kΩ)
- P4 connector has onboard 10kΩ pull-ups
- CR2032 battery keeps time when power is off

---

## 2. DS18B20 Temperature Sensors - Bus #1 (Internal Motors)

**Connector:** P3 (SPI Peripheral - 6-pin 1.25mm JST)  
**Pin Used:** Pin 3 (GPIO21 - CS)

```
DS18B20 Sensors      P3 Connector              Pull-up Resistor
┌─────────────┐     ┌───────────────┐          ┌──────────┐
│ RED (VCC) ──┼─────┤ Pin 1 (VCC)   │ → 3.3V   │          │
│ BLK (GND) ──┼─────┤ Pin 2 (GND)   │ → Ground │  4.7kΩ   │
│ YEL (DATA)──┼─────┤ Pin 3 (GPIO21)├──────────┤          ├──→ VCC
└─────────────┘     └───────────────┘          └──────────┘
                    Pins 4-6: Not used for temp
                    
Multiple Sensors (Parallel):
┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐
│ X    │  │ YL   │  │ YR   │  │ Z    │  ← Motor Driver Labels
│DS18B2│  │DS18B2│  │DS18B2│  │DS18B2│
└──┬───┘  └──┬───┘  └──┬───┘  └──┬───┘
   │         │         │         │
   └─────────┴─────────┴─────────┴──→ GPIO21 (with 4.7kΩ pull-up)
```

**Component:** BOJACK DS18B20 Module (includes built-in pull-up)
- Waterproof stainless steel probe
- 1-meter cable
- Clips directly to motor driver heatsinks
- No external resistor needed if using adapter module!

---

## 3. DS18B20 Temperature Sensors - Bus #2 (External Motors)

**Connection:** GPIO26 (requires jumper wire from Audio DAC pad)

```
Audio Connector Area (bottom of board):
┌─────────────────────────────────┐
│  ┌───┐  ┌───┐    JP1 (Speaker)  │
│  │   │  │   │     [1] [2]       │
│  └───┘  └───┘                    │
│   U5    Audio                    │
│  Amp    Caps     GPIO26 Pad◄─────┼─── Solder wire here
│                    (DAC)         │
└─────────────────────────────────┘

External Sensors     GPIO26 Wire           Pull-up Resistor
┌─────────────┐     ┌──────────┐          ┌──────────┐
│ RED (VCC) ──┼─────┤ 3.3V     │          │          │
│ BLK (GND) ──┼─────┤ GND      │          │  4.7kΩ   │
│ YEL (DATA)──┼─────┤ GPIO26   ├──────────┤          ├──→ 3.3V
└─────────────┘     └──────────┘          └──────────┘
```

**Notes:**
- Requires soldering jumper wire to GPIO26 pad
- Alternative: Use P3 pins 4-6 if not using SD card
- Up to 4 additional sensors per bus

---

## 4. Fan Control - PWM Signal

**Connection:** GPIO4 (requires jumper wire from Audio Enable pad)

```
Audio Section (bottom of board):
┌─────────────────────┐
│  GPIO4 Pad◄─────────┼─── Solder wire here (Audio Enable)
│    (EN)             │
│                     │
│   U5 Audio Amp      │
└─────────────────────┘

Fan PWM Connection:
GPIO4 Wire ──→ ┌─────────────────┐
               │   PWM FAN        │
               │   Control Input  │ ← Usually blue/yellow wire
               │   (4-pin fan)    │
               └─────────────────┘
```

**Fan Pinout (Standard 4-pin):**
1. GND (Black) → External 12V GND
2. +12V (Yellow/Red) → External 12V power
3. TACH (Yellow) → GPIO35 (see below)
4. PWM (Blue) → GPIO4

---

## 5. Fan Control - Tachometer (RPM Sensing)

**Connector:** P2 (Expansion Input - 2-pin 1.25mm JST)

```
Fan Tach Wire        P2 Connector         Pull-up
┌──────────┐        ┌──────────┐         ┌────────┐
│ TACH     ├────────┤ Pin 1    │         │ 10kΩ   │
│ (Yellow) │        │ (GPIO35) ├─────────┤        ├──→ 3.3V
└──────────┘        └──────────┘         └────────┘
                    Pin 2: NC (GPIO39 spare)

Complete Fan Wiring:
                    External 12V PSU
                    ┌────┬────┐
                    │ +  │ -  │
                    └─┬──┴─┬──┘
                      │    │
    ┌─────────────────┤    ├────────────────────┐
    │                 │    │                    │
┌───┴────┐            │    │              ┌─────┴─────┐
│ +12V   │◄───────────┘    └─────────────►│ GND       │
│ GND    │◄──────────────────────────────►│           │
│ TACH   ├────────────────────────────────►│ P2 Pin 1  │ CYD
│ PWM    │◄───────────────────────────────┤ GPIO4     │ Board
└────────┘                                 └───────────┘
4-Pin Fan
```

**Notes:**
- GPIO35 is input-only (perfect for tach)
- 10kΩ pull-up resistor required
- Tach outputs 2 pulses per revolution typically

---

## 6. PSU Voltage Monitor

**Connection:** GPIO34 (requires jumper wire from Battery ADC pad)

```
Battery Section (bottom of board):
┌──────────────────────┐
│ BAT+ Connector       │
│   [1] [2]            │
│                      │
│  GPIO34 Pad◄─────────┼─── Solder wire here (BAT_ADC)
│    (ADC)             │
└──────────────────────┘

Voltage Divider Circuit:
24V PSU                                    CYD Board
┌────┐                                    ┌──────────┐
│ +  ├───┬───[100kΩ R1]───┬──────────────┤ GPIO34   │
│    │   │                 │              │ (ADC)    │
│ -  ├───┴─────────────────┼──[10kΩ R2]──┤ GND      │
└────┘                     │              └──────────┘
                           │
                        Vout = 3.3V max
                        @ 33V input

Calculation:
R1 = 100kΩ (to PSU+)
R2 = 10kΩ (to GND)
Voltage divider: Vout = Vin × (R2 / (R1 + R2))
Max safe input: 33V → 3.3V at ADC
Calibration factor: (R1 + R2) / R2 = 110k / 10k = 11.0
But use cfg.psu_voltage_cal = 7.3 (accounts for CYD voltage regulator offset)
```

**Component Values:**
- R1: 100kΩ resistor (Brown-Black-Yellow-Gold)
- R2: 10kΩ resistor (Brown-Black-Orange-Gold)
- Both 1/4W or better

**Safety:**
- Never exceed 33V input!
- For 24V nominal PSU, max ~27V = safe
- Double-check resistor values with multimeter

---

## 7. Power Supply

```
┌─────────────────────────────────────────────────────────────┐
│                       Power Distribution                     │
└─────────────────────────────────────────────────────────────┘

Main Power (CYD Board):
  USB Type-C ──→ 5V @ 500mA min (1A recommended)
  
External 12V (for Fan):
  12V PSU ──→ Fan directly (NOT to CYD board)
  
Complete Power Diagram:
                              ┌───────────────┐
USB 5V ──→ TYPE-C ──→ CYD ──→ │ • ESP32       │
                     Board    │ • LCD Display │
                              │ • SD Card     │
                              │ • Sensors     │
                              └───────────────┘
                              
                              ┌───────────────┐
12V PSU ─┬─→ Fan Power ──→    │ 4-Pin PWM Fan │
         │                    └───────────────┘
         │                    ┌───────────────┐
         └─→ Voltage ──→      │ PSU Voltage   │
             Divider          │ Monitor       │
                              └───────────────┘
```

**Power Budget:**
- CYD Board: ~300mA @ 5V = 1.5W
- DS18B20 (4x): ~4mA total
- DS3231 RTC: 0.1mA
- **Total 5V:** ~310mA (safe with 500mA USB)

**External 12V:**
- Fan: ~100-500mA (varies with speed)
- Use separate 12V supply (NOT from USB)

---

## 8. Complete External Wiring Summary

```
┌─────────────────────────────────────────────────────────────┐
│  Component          Connection        Notes                 │
├─────────────────────────────────────────────────────────────┤
│  DS3231 RTC         P4 Connector      Standard I2C          │
│  DS18B20 Bus 1      P3 Pin 3          4 sensors max         │
│  DS18B20 Bus 2      GPIO26 pad        Requires solder       │
│  Fan PWM            GPIO4 pad         Requires solder       │
│  Fan Tach           P2 Pin 1          10kΩ pull-up          │
│  PSU Voltage        GPIO34 pad        Voltage divider       │
│  Power (CYD)        USB Type-C        5V @ 1A               │
│  Power (Fan)        External 12V      Separate supply       │
└─────────────────────────────────────────────────────────────┘
```

---

## 9. Connector Pin Reference

### P2 - Expansion Input (2-pin 1.25mm JST)
```
Pin 1: GPIO35 (input only) → Fan Tach
Pin 2: GPIO39 (input only) → Spare
```

### P3 - SPI Peripheral (6-pin 1.25mm JST)
```
Pin 1: VCC (3.3V out)
Pin 2: GND
Pin 3: CS (GPIO21) → DS18B20 Bus 1 DATA
Pin 4: SCK (GPIO18) - Shared with SD
Pin 5: MISO (GPIO19) - Shared with SD
Pin 6: MOSI (GPIO23) - Shared with SD
```

### P4 - I2C Peripheral (6-pin 1.25mm JST)
```
Pin 1: VCC (3.3V out)
Pin 2: GND
Pin 3: NC
Pin 4: SDA (GPIO32) → RTC SDA
Pin 5: SCL (GPIO25) → RTC SCL
Pin 6: NC
```

---

## 10. Soldering Requirements

### Required Solder Points:
1. **GPIO26** (Audio DAC pad) → DS18B20 Bus 2
2. **GPIO4** (Audio Enable pad) → Fan PWM
3. **GPIO34** (Battery ADC pad) → PSU Voltage Divider

### Soldering Tips:
- Use thin wire (22-24 AWG)
- Keep wires short to reduce noise
- Use heat shrink tubing
- Label each wire!
- Test continuity before assembly

---

## 11. Pre-Flight Checklist

Before powering on:

✅ All solder joints clean and secure  
✅ No solder bridges between pads  
✅ Voltage divider resistor values confirmed  
✅ Pull-up resistors installed (4.7kΩ for temp, 10kΩ for tach)  
✅ DS3231 has CR2032 battery installed  
✅ DS18B20 sensors labeled (X, YL, YR, Z)  
✅ Fan wires connected correctly (check polarity!)  
✅ USB cable is good quality (1A capable)  
✅ SD card formatted FAT32  
✅ No shorts between 3.3V and GND  

---

## 12. Testing Procedure

### Step 1: Power On (No external connections)
- USB connected only
- RGB LED should turn white, then yellow
- Display shows "FluidDash Initializing..."
- No magic smoke!

### Step 2: Connect RTC
- Power off
- Connect P4 to RTC
- Power on
- Serial monitor: "RTC initialized"

### Step 3: Connect Temperature Sensors
- Power off
- Connect P3 to DS18B20 Bus 1
- Power on
- Serial monitor: "Found X internal temp sensors"
- Display shows temperatures

### Step 4: Connect Fan
- Power off
- Connect fan tach to P2
- Connect fan PWM to GPIO4
- Connect fan power to external 12V
- Power on
- Fan should spin at minimum speed
- Display shows RPM

### Step 5: Connect PSU Monitor
- Power off
- Wire voltage divider
- Power on
- Display shows PSU voltage (should read ~24V)

---

## 13. Troubleshooting

### No Temperature Readings
- Check pull-up resistor (4.7kΩ)
- Verify power to sensors (3.3V)
- Check DATA wire connection
- Try different sensor (may be faulty)

### Fan Not Spinning
- Check 12V power to fan
- Verify PWM signal with oscilloscope/multimeter
- Check GPIO4 solder joint
- Try manual PWM test (100% duty)

### No RPM Reading
- Check pull-up resistor (10kΩ) on tach line
- Verify tach wire connection
- Some fans need minimum speed to generate tach signal

### Wrong PSU Voltage
- Double-check resistor values (100kΩ and 10kΩ)
- Adjust cfg.psu_voltage_cal value
- Measure actual voltage at GPIO34 (should be ≤3.3V)

### RTC Not Found
- Check I2C wiring (SDA/SCL correct?)
- Try swapping SDA/SCL (easy to mix up)
- Check power to RTC module
- Run I2C scanner code

---

## 14. Cable Specifications

| Cable Type | Length | Gauge | Purpose |
|------------|--------|-------|---------|
| USB Type-C | 3-6 ft | 24/28 | Power + Programming |
| DS18B20 | 1m | Included | Temperature sensing |
| Fan Cable | 12" | 24 AWG | PWM + Tach |
| RTC I2C | 6" | 26 AWG | Clock module |
| PSU Sense | 12" | 24 AWG | Voltage divider |

---

## 15. Enclosure & Mounting

**Recommended Mounting:**
- CYD board: Front panel mount (4x M3 screws)
- RTC module: Inside enclosure (near CYD)
- DS18B20 sensors: On motor driver heatsinks
- Fan: Rear panel exhaust
- Voltage divider: Inside enclosure (insulated!)

**Enclosure Size:**
- Minimum: 120mm × 100mm × 40mm
- Recommended: 150mm × 120mm × 50mm
- Material: 3D printed PLA/PETG or laser-cut acrylic

---

This completes the wiring guide. For additional help, see:
- `FLUIDDASH_HARDWARE_REFERENCE.md`
- LCD Wiki documentation at lcdwiki.com
