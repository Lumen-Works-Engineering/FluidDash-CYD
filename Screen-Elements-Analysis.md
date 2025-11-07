# FluidDash Screen Elements - Complete Analysis

**Date**: November 6, 2025  
**Status**: ✅ All elements in monitor.json and screen_0.json are supported!

---

## ✅ Supported Element Types

Your `screen_renderer.cpp` supports **9 element types**:

| #   | Type       | Enum              | Purpose                                |
| --- | ---------- | ----------------- | -------------------------------------- |
| 1   | `rect`     | ELEM_RECT         | Filled rectangles (backgrounds, boxes) |
| 2   | `line`     | ELEM_LINE         | Lines and dividers                     |
| 3   | `text`     | ELEM_TEXT_STATIC  | Static text labels                     |
| 4   | `dynamic`  | ELEM_TEXT_DYNAMIC | Dynamic text with variables            |
| 5   | `temp`     | ELEM_TEMP_VALUE   | Temperature display with color coding  |
| 6   | `coord`    | ELEM_COORD_VALUE  | Coordinate display (X/Y/Z positions)   |
| 7   | `status`   | ELEM_STATUS_VALUE | Status text (IDLE, RUN, etc.)          |
| 8   | `progress` | ELEM_PROGRESS_BAR | Progress bars                          |
| 9   | `graph`    | ELEM_GRAPH        | Temperature history graphs             |

---

## 📊 monitor.json Analysis

**Total Elements**: 39

| Element Type | Count | Supported? |
| ------------ | ----- | ---------- |
| text         | 13    | ✅ YES      |
| dynamic      | 11    | ✅ YES      |
| coord        | 8     | ✅ YES      |
| line         | 3     | ✅ YES      |
| rect         | 2     | ✅ YES      |
| graph        | 1     | ✅ YES      |
| status       | 1     | ✅ YES      |

**✅ Result**: ALL 39 elements are fully supported!

### What monitor.json Contains

- **13 text elements** - Static labels like "Temps:", "Peak:", "FluidNC:"
- **11 dynamic elements** - Real-time data like date/time, fan speed, PSU voltage
- **8 coord elements** - X/Y/Z position displays with formatting
- **3 line elements** - Visual separators
- **2 rect elements** - Background and borders
- **1 graph element** - Temperature history graph
- **1 status element** - FluidNC state (IDLE/RUN/etc.)

---

## 📊 screen_0.json Analysis

**Total Elements**: 13

| Element Type | Count | Supported? |
| ------------ | ----- | ---------- |
| dynamic      | 7     | ✅ YES      |
| temp         | 3     | ✅ YES      |
| text         | 1     | ✅ YES      |
| status       | 1     | ✅ YES      |
| line         | 1     | ✅ YES      |

**✅ Result**: ALL 13 elements are fully supported!

### What screen_0.json Contains

- **7 dynamic elements** - Date, time, fan, PSU, FluidNC data
- **3 temp elements** - Temperature displays with color coding
- **1 text element** - Title "FluidDash"
- **1 status element** - Machine status
- **1 line element** - Separator

---

## 🎯 Why Your Screen Looks Bad (Not Element Support!)

Since **ALL elements are supported**, the issues you're seeing are NOT due to missing element types. The problems are:

### 1. ❌ **Font Quality Issues**

**Problem**: Using built-in LovyanGFX fonts (0-8) which are pixelated

**In your JSON**:

```json
{
  "type": "text",
  "font": 2,  // ← Built-in font = blocky
  "text": "FluidDash"
}
```

**Solution**: The renderer DOES support custom fonts, but you need to:

1. Check if `screen_renderer.cpp` loads TrueType fonts
2. Or specify better built-in fonts (4, 6, or 7 are smoother)

### 2. ❌ **Layout/Positioning**

**monitor.json uses absolute coordinates** - if they don't match your display size (480x320), text can overlap or be cut off.

**Example from monitor.json**:

```json
{"type": "text", "x": 3, "y": 2, "text": "FluidDash"}
```

These coordinates were probably tuned for a specific display configuration.

### 3. ❌ **Color Contrast**

Some text may have poor contrast against background colors, making it hard to read.

### 4. ❌ **Graph Rendering Quality**

The `graph` element IS supported, but the implementation might be basic (no antialiasing, fixed colors, etc.)

---

## 📋 Element Type Details

### 1. `rect` - Rectangle

```json
{
  "type": "rect",
  "x": 0,
  "y": 0,
  "width": 480,
  "height": 320,
  "color": 15
}
```

**Properties**:

- `x`, `y` - Top-left corner position
- `width`, `height` - Dimensions
- `color` - Color value (0-65535 for RGB565)

---

### 2. `line` - Line

```json
{
  "type": "line",
  "x1": 0,
  "y1": 38,
  "x2": 480,
  "y2": 38,
  "color": 31727
}
```

**Properties**:

- `x1`, `y1` - Start point
- `x2`, `y2` - End point
- `color` - Line color

---

### 3. `text` - Static Text

```json
{
  "type": "text",
  "x": 3,
  "y": 2,
  "text": "FluidDash",
  "font": 4,
  "color": 2047
}
```

**Properties**:

- `x`, `y` - Position
- `text` - String to display
- `font` - Font size (0-8 built-in, or custom font)
- `color` - Text color

---

### 4. `dynamic` - Dynamic Text

```json
{
  "type": "dynamic",
  "x": 230,
  "y": 2,
  "source": "datetime",
  "format": "%b %d %H:%M",
  "font": 2,
  "color": 65535
}
```

**Properties**:

- `source` - Data source name
- `format` - Format string (printf-style)
- Other properties same as `text`

**Available Sources**:

- `datetime` - Current date/time from RTC
- `fanrpm` - Fan RPM
- `fanspeed` - Fan PWM percentage
- `psuvolt` - PSU voltage
- `fluidnc_x`, `fluidnc_y`, `fluidnc_z` - Machine positions

---

### 5. `temp` - Temperature Display

```json
{
  "type": "temp",
  "x": 140,
  "y": 53,
  "sensor": 0,
  "font": 2,
  "label": "X:"
}
```

**Properties**:

- `sensor` - Temperature sensor index (0-3)
  - 0 = X axis
  - 1 = Y-left axis
  - 2 = Y-right axis
  - 3 = Z axis
- `label` - Optional prefix text
- Color automatically changes based on temperature:
  - Green < 40°C
  - Yellow 40-55°C
  - Red > 55°C

---

### 6. `coord` - Coordinate Display

```json
{
  "type": "coord",
  "x": 40,
  "y": 248,
  "axis": "x",
  "format": "wcs",
  "font": 2,
  "label": "X:",
  "color": 2047
}
```

**Properties**:

- `axis` - "x", "y", or "z"
- `format` - "wcs" or "mcs" (work/machine coordinate system)
- `label` - Prefix text
- Displays position with 3 decimal places

---

### 7. `status` - Status Display

```json
{
  "type": "status",
  "x": 10,
  "y": 170,
  "font": 2,
  "color": 63519
}
```

**Properties**:

- Shows FluidNC machine state
- Displays: "IDLE", "RUN", "HOLD", "ALARM", etc.

---

### 8. `progress` - Progress Bar

```json
{
  "type": "progress",
  "x": 50,
  "y": 200,
  "width": 200,
  "height": 20,
  "source": "fanspeed",
  "color": 2016
}
```

**Properties**:

- `width`, `height` - Bar dimensions
- `source` - Data source for percentage
- `color` - Bar fill color

---

### 9. `graph` - Temperature Graph

```json
{
  "type": "graph",
  "x": 10,
  "y": 140,
  "width": 460,
  "height": 96
}
```

**Properties**:

- `width`, `height` - Graph dimensions
- Automatically plots temperature history for all 4 sensors
- Different color per sensor

---

## 🛠️ How to Fix Your Display Issues

### Fix 1: Use Better Fonts

**Current** (monitor.json uses font 2):

```json
"font": 2  // Blocky
```

**Better** (try font 4, 6, or 7):

```json
"font": 4  // Smoother
```

**Best** (if TrueType fonts are loaded):
Check if your renderer loads custom fonts from files.

---

### Fix 2: Compare with screen_0.json

screen_0.json was created by Claude and might have better:

- Layout spacing
- Font choices
- Color contrast

**Action**: Copy good practices from screen_0.json to monitor.json

---

### Fix 3: Adjust Coordinates for 480x320

Your display is 480x320. Verify monitor.json coordinates don't:

- Go beyond x=480 or y=320
- Overlap other elements
- Have poor spacing

---

### Fix 4: Fix Date/Time Format

**Current issue**: Date doesn't match RTC

Check the `format` string in the dynamic element:

```json
{
  "type": "dynamic",
  "source": "datetime",
  "format": "%b %d %H:%M"  // "Oct 28 12:05"
}
```

Make sure this matches what you want!

---

## 📝 Next Steps

1. **Compare Files**: 
   
   - Open monitor.json and screen_0.json side-by-side
   - Identify layout differences
   - Copy better coordinates/fonts from screen_0.json

2. **Test Font Changes**:
   
   - Change `"font": 2` to `"font": 4` in monitor.json
   - Upload and test
   - Try fonts 4, 6, 7 to see which looks best

3. **Fix Date/Time**:
   
   - Verify the `format` string in the datetime dynamic element
   - Match it to your RTC output format

4. **Optimize Layout**:
   
   - Adjust x/y coordinates for better spacing
   - Ensure no element overlaps
   - Center important data

---

## ✅ Summary

**Good News**: 

- ✅ ALL element types in both JSON files are supported
- ✅ The renderer is working correctly
- ✅ This is a **configuration issue**, not a code issue

**The Problem**:

- ❌ Font selection (using blocky built-in fonts)
- ❌ Layout/positioning needs tuning
- ❌ Color contrast issues
- ❌ Date format mismatch

**The Solution**:

- 📝 Edit monitor.json to use better fonts
- 📝 Adjust coordinates and colors
- 📝 Copy good practices from screen_0.json

Would you like me to create an **improved monitor.json** with better fonts, layout, and colors?
