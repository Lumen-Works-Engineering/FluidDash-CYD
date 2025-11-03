# PROGMEM HTML Optimization - Claude Code Instructions

## Objective
Convert web server HTML string generation from RAM-based concatenation to PROGMEM storage, freeing ~20-40KB of RAM.

## Current State
- main.cpp contains 4 HTML generation functions using String concatenation
- Each function builds HTML dynamically, consuming RAM
- Target functions: getMainHTML(), getSettingsHTML(), getAdminHTML(), getWiFiConfigHTML()

## Expected Outcome
- HTML stored in Flash memory (PROGMEM) instead of RAM
- ~20-40KB RAM freed
- More readable HTML (proper formatting)
- Functions use template placeholders for dynamic content

---

## PHASE 1: Convert getMainHTML()

### Step 1.1: Locate Function
Find the `getMainHTML()` function in main.cpp. It should look like:
```cpp
String getMainHTML() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  // ... many concatenations
  return html;
}
```

### Step 1.2: Extract HTML Content
Copy all the HTML content being built (everything between quotes)

### Step 1.3: Create PROGMEM Constant
Add this BEFORE the function (after includes, before other functions):

```cpp
// ============ WEB SERVER HTML TEMPLATES (PROGMEM) ============

const char MAIN_HTML[] PROGMEM = R"rawliteral(
[PASTE EXTRACTED HTML HERE - properly formatted]
)rawliteral";
```

### Step 1.4: Replace Function Body
Change the function to:
```cpp
String getMainHTML() {
  return String(FPSTR(MAIN_HTML));
}
```

### Step 1.5: Test
- Compile: `pio run`
- Check: RAM usage should decrease slightly
- Verify: Main page loads in browser

**CHECKPOINT:** Compilation must succeed before continuing.

---

## PHASE 2: Convert getSettingsHTML()

### Step 2.1: Identify Dynamic Content
In the original function, find all dynamic values like:
- `cfg.device_name`
- `cfg.fluidnc_ip`
- Checkbox states
- Selected options

### Step 2.2: Replace with Placeholders
In the HTML, replace dynamic content with placeholders:
```html
<input type='text' name='device_name' value='%DEVICE_NAME%'>
<input type='checkbox' name='auto_discover' %AUTO_DISCOVER_CHECKED%>
<option value='1' %SELECTED_F%>Fahrenheit</option>
```

### Step 2.3: Create PROGMEM Constant
Add after MAIN_HTML:
```cpp
const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
[PASTE FORMATTED HTML WITH PLACEHOLDERS]
)rawliteral";
```

### Step 2.4: Replace Function
```cpp
String getSettingsHTML() {
  String html = String(FPSTR(SETTINGS_HTML));
  
  // Replace all placeholders
  html.replace("%DEVICE_NAME%", cfg.device_name);
  html.replace("%FLUIDNC_IP%", cfg.fluidnc_ip);
  html.replace("%FLUIDNC_PORT%", String(cfg.fluidnc_port));
  html.replace("%AUTO_DISCOVER_CHECKED%", cfg.fluidnc_auto_discover ? "checked" : "");
  // ... add all other replacements
  
  return html;
}
```

### Step 2.5: Common Patterns for Replacements

**Checkboxes:**
```cpp
html.replace("%CHECKBOX_NAME%", configValue ? "checked" : "");
```

**Radio/Select Options:**
```cpp
html.replace("%OPTION_1%", configValue == 1 ? "selected" : "");
html.replace("%OPTION_2%", configValue == 2 ? "selected" : "");
```

**Numeric Values:**
```cpp
html.replace("%NUMBER%", String(configValue));
```

**Float Values:**
```cpp
html.replace("%FLOAT%", String(configValue, decimalPlaces));
```

### Step 2.6: Test
- Compile: `pio run`
- Verify: Settings page loads
- Check: All form fields show correct values
- Test: Save settings and verify persistence

**CHECKPOINT:** All settings must display correctly before continuing.

---

## PHASE 3: Convert getAdminHTML()

### Step 3.1: Follow Same Pattern
1. Extract HTML content
2. Add placeholders for dynamic content
3. Create PROGMEM constant
4. Update function with replacements
5. Test thoroughly

### Step 3.2: Add PROGMEM Constant
```cpp
const char ADMIN_HTML[] PROGMEM = R"rawliteral(
[PASTE FORMATTED HTML WITH PLACEHOLDERS]
)rawliteral";
```

### Step 3.3: Update Function
```cpp
String getAdminHTML() {
  String html = String(FPSTR(ADMIN_HTML));
  // Add all placeholder replacements
  return html;
}
```

### Step 3.4: Test
- Compile and verify admin page

**CHECKPOINT:** Admin page must work before continuing.

---

## PHASE 4: Convert getWiFiConfigHTML()

### Step 4.1: Same Process
1. Extract HTML
2. Add placeholders if needed
3. Create PROGMEM constant
4. Update function
5. Test

### Step 4.2: Add PROGMEM Constant
```cpp
const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
[PASTE FORMATTED HTML]
)rawliteral";
```

### Step 4.3: Update Function
```cpp
String getWiFiConfigHTML() {
  return String(FPSTR(WIFI_CONFIG_HTML));
}
```

### Step 4.4: Test
- Compile and verify WiFi config page

**CHECKPOINT:** All pages must work.

---

## PHASE 5: Organize and Clean Up

### Step 5.1: Group PROGMEM Constants
Organize all PROGMEM strings together:
```cpp
// ============ WEB SERVER HTML TEMPLATES (PROGMEM) ============

const char MAIN_HTML[] PROGMEM = R"rawliteral(
...
)rawliteral";

const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
...
)rawliteral";

const char ADMIN_HTML[] PROGMEM = R"rawliteral(
...
)rawliteral";

const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
...
)rawliteral";

// ============ WEB SERVER FUNCTIONS ============
```

### Step 5.2: Add Comments
Add brief comments explaining each template's purpose

### Step 5.3: Final Compilation
- Run: `pio run -t clean`
- Run: `pio run`
- Note RAM usage
- Compare to pre-optimization RAM

---

## PHASE 6: Testing & Verification

### Test Checklist
Execute each test:

- [ ] Compile successful (zero errors)
- [ ] Main page loads
- [ ] Settings page loads
- [ ] Admin page loads  
- [ ] WiFi config page loads
- [ ] All form fields show correct values
- [ ] Can save settings
- [ ] Settings persist after reboot
- [ ] Navigation between pages works
- [ ] No watchdog resets
- [ ] RAM usage decreased

### Measure Impact
Record and report:
```
BEFORE optimization:
RAM:   [=============     ] XX.X% (used XXXXXX bytes)
Flash: [============      ] XX.X% (used XXXXXXX bytes)

AFTER optimization:
RAM:   [========          ] XX.X% (used XXXXXX bytes)  ✅ -XXXX bytes
Flash: [============      ] XX.X% (used XXXXXXX bytes)  ⚠️ +XXXX bytes
```

---

## Critical Rules

### 1. ALWAYS Use FPSTR()
```cpp
// ❌ WRONG - Won't work!
String html = String(MAIN_HTML);

// ✅ CORRECT
String html = String(FPSTR(MAIN_HTML));
```

### 2. Test After Each Phase
Don't move to next function until current one compiles and works

### 3. Preserve All Functionality
HTML must display exactly the same as before
All dynamic content must update correctly

### 4. Handle Missing Placeholders
If a placeholder is forgotten, it will show in the HTML
Double-check all %PLACEHOLDER% values are replaced

---

## Common Issues & Solutions

### Issue: "undefined reference to FPSTR"
**Solution:** Missing include, add:
```cpp
#include <pgmspace.h>
```

### Issue: Page loads but shows %PLACEHOLDER%
**Solution:** Forgot to add .replace() call for that placeholder

### Issue: Compilation error about "rawliteral"
**Solution:** Check R"rawliteral( and )rawliteral" syntax is exact

### Issue: RAM usage didn't decrease
**Solution:** 
- Verify FPSTR() is used (not just String())
- Check old concatenation code is fully removed
- Measure after upload, not just compilation

---

## Success Criteria

Optimization is complete when:
- ✅ All 4 HTML functions converted to PROGMEM
- ✅ Zero compilation errors
- ✅ All web pages load correctly
- ✅ All dynamic content displays properly
- ✅ Settings can be saved and persist
- ✅ RAM usage decreased by 15-40KB
- ✅ Flash usage increased by ~10KB (acceptable)
- ✅ No functionality lost

---

## Completion Report Format

After finishing, provide this report:

```
PROGMEM HTML Optimization Complete
===================================

Functions Converted:
- [✓] getMainHTML()
- [✓] getSettingsHTML()  
- [✓] getAdminHTML()
- [✓] getWiFiConfigHTML()

Memory Impact:
- RAM: [before] → [after] (△ [change])
- Flash: [before] → [after] (△ [change])

Testing Results:
- [✓] All pages load
- [✓] All forms work
- [✓] Settings persist
- [✓] No errors

Files Modified:
- main.cpp (4 functions converted, 4 PROGMEM constants added)

Lines Changed:
- Removed: ~XXX lines (concatenation code)
- Added: ~XXX lines (PROGMEM templates)
- Net: ~XXX lines (reduction)
```

---

## Reference

For detailed explanations and advanced patterns, see:
- WEB_SERVER_PROGMEM_GUIDE.md (comprehensive tutorial)

For questions about the PROGMEM approach, ask the user.

---

**Start with Phase 1 and proceed sequentially. Test after each phase. Report progress after completing each phase.**
