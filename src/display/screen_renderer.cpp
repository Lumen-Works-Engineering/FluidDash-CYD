#include "screen_renderer.h"
#include "display.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include "../storage_manager.h"

// External variables from main.cpp (needed for data access)
extern bool sdCardAvailable;
extern StorageManager storage;
extern float temperatures[4];
extern float posX, posY, posZ, posA;
extern float wposX, wposY, wposZ, wposA;
extern int feedRate;
extern int spindleRPM;
extern float psuVoltage;
extern uint8_t fanSpeed;
extern String machineState;
extern RTC_DS3231 rtc;
extern bool rtcAvailable;
extern float *tempHistory;
extern uint16_t historySize;
extern uint16_t historyIndex;

// Smart font selection based on text size
void selectBestFont(int textSize) {
    if (textSize >= 2) {
        gfx.setFont(&fonts::Font4);  // Smooth font for size 2+
    } else {
        gfx.setFont(&fonts::Font2);  // Fallback font for size 1
    }
}

// Replace ambiguous/undefined createDocument(...) with a proper ArduinoJson document.
// Use heap allocation to avoid large static/global stack usage.
DynamicJsonDocument doc(8192);

// Replace applyFontFromJson with safer reads (use .as<T>() and null checks)
// Replace the previous setFont(JsonObject& elem) implementation with a safe, internal variant-aware helper.
static void applyFontFromJson(JsonVariantConst jElem) {
    // Default size
    int size = 2;
    if (!jElem.isNull()) {
        JsonVariantConst sizeVar = jElem["size"];
        if (!sizeVar.isNull()) size = sizeVar.as<int>();
    }

    // Safe C-string read for font name
    const char* fontName = nullptr;
    if (!jElem.isNull()) {
        JsonVariantConst fn = jElem["font"];
        if (!fn.isNull()) fontName = fn.as<const char*>();
    }
    if (fontName == nullptr) fontName = "";

    // Compare known font names using strcmp to avoid Arduino String conversion issues
    if (strlen(fontName) > 0) {
        if (strcmp(fontName, "FreeSans12pt7b") == 0) {
            selectBestFont(size);
        } else {
            // Unknown named font -> fallback behavior
            selectBestFont(size);
        }
    } else {
        // Numeric or missing font field -> fallback
        selectBestFont(size);
    }
}

// Since GraphElement is used like other element types, we should use ScreenElement instead
void renderGraph(const ScreenElement& el) {
    // Draw temperature graph
    gfx.fillRect(el.x, el.y, el.w, el.h, el.bgColor);
    gfx.drawRect(el.x, el.y, el.w, el.h, el.color);

    if (tempHistory != nullptr && historySize > 0) {
        float minTemp = 10.0;
        float maxTemp = 60.0;

        // Draw temperature line
        for (int i = 1; i < historySize; i++) {
            int idx1 = (historyIndex + i - 1) % historySize;
            int idx2 = (historyIndex + i) % historySize;

            float temp1 = tempHistory[idx1];
            float temp2 = tempHistory[idx2];

            int x1 = el.x + ((i - 1) * el.w / historySize);
            int y1 = el.y + el.h - ((temp1 - minTemp) / (maxTemp - minTemp) * el.h);
            int x2 = el.x + (i * el.w / historySize);
            int y2 = el.y + el.h - ((temp2 - minTemp) / (maxTemp - minTemp) * el.h);

            y1 = constrain(y1, el.y, el.y + el.h);
            y2 = constrain(y2, el.y, el.y + el.h);

            gfx.drawLine(x1, y1, x2, y2, el.color);
        }
    }
}

// Add: simple month abbreviation helper used by formatDateTime
static const char* monthShortStr(uint8_t month) {
    static const char* months[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    if (month >= 1 && month <= 12) return months[month - 1];
    return "???";
}

String formatDateTime(const DateTime& dt) {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%s %02d %02d:%02d", 
    monthShortStr(dt.month()), dt.day(), dt.hour(), dt.minute());
  return String(buffer);
}

// Convert hex color string to uint16_t RGB565
uint16_t parseColor(const char* hexColor) {
    if (hexColor == nullptr || strlen(hexColor) < 4) {
        return 0x0000; // Default to black
    }

    // Skip '#' if present
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;

    // Parse hex string
    uint32_t color = strtoul(hex, nullptr, 16);

    // Convert to RGB565
    if (strlen(hex) == 4) {
        // Short form: RGB -> RRGGBB
        uint8_t r = ((color >> 8) & 0xF) * 17;
        uint8_t g = ((color >> 4) & 0xF) * 17;
        uint8_t b = (color & 0xF) * 17;
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    } else {
        // Full form: RRGGBB
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}

// Parse element type from string
ElementType parseElementType(const char* typeStr) {
    if (strcmp(typeStr, "rect") == 0) return ELEM_RECT;
    if (strcmp(typeStr, "line") == 0) return ELEM_LINE;
    if (strcmp(typeStr, "text") == 0) return ELEM_TEXT_STATIC;
    if (strcmp(typeStr, "dynamic") == 0) return ELEM_TEXT_DYNAMIC;
    if (strcmp(typeStr, "temp") == 0) return ELEM_TEMP_VALUE;
    if (strcmp(typeStr, "coord") == 0) return ELEM_COORD_VALUE;
    if (strcmp(typeStr, "status") == 0) return ELEM_STATUS_VALUE;
    if (strcmp(typeStr, "progress") == 0) return ELEM_PROGRESS_BAR;
    if (strcmp(typeStr, "graph") == 0) return ELEM_GRAPH;
    return ELEM_NONE;
}

// Parse text alignment from string
TextAlign parseAlignment(const char* alignStr) {
    if (strcmp(alignStr, "center") == 0) return ALIGN_CENTER;
    if (strcmp(alignStr, "right") == 0) return ALIGN_RIGHT;
    return ALIGN_LEFT;
}
// Validate and normalize data source names
bool validateDataSource(const char* source, char* normalizedOut, size_t outSize) {
    if (!source || strlen(source) == 0) {
        return false;
    }

    // Known valid data sources
    const char* validSources[] = {
        "rtcDateTime", "dateTime",  // Accept both, normalize to rtcDateTime
        "temp0", "temp1", "temp2", "temp3",
        "temp0Peak", "temp1Peak", "temp2Peak", "temp3Peak",
        "fanSpeed", "fanRPM",
        "psuVoltage",
        "machineState", "feedRate", "spindleRPM",
        "wposX", "wposY", "wposZ", "wposA",
        "posX", "posY", "posZ", "posA",
        "ipAddress", "ssid", "deviceName", "fluidncIP"
    };

    // Check if source matches any valid source
    for (int i = 0; i < sizeof(validSources) / sizeof(validSources[0]); i++) {
        if (strcmp(source, validSources[i]) == 0) {
            // Normalize dateTime -> rtcDateTime
            if (strcmp(source, "dateTime") == 0) {
                strncpy(normalizedOut, "rtcDateTime", outSize - 1);
            } else {
                strncpy(normalizedOut, source, outSize - 1);
            }
            normalizedOut[outSize - 1] = '\\0';
            return true;
        }
    }

    Serial.printf("[JSON] WARNING: Unknown data source '%s'\\n", source);
    Serial.println("[JSON] Known sources: rtcDateTime, temp0-3, fanSpeed, psuVoltage, etc.");

    // Still copy it (might be added later)
    strncpy(normalizedOut, source, outSize - 1);
    normalizedOut[outSize - 1] = '\\0';
    return false;
}

// Load screen configuration from JSON file
bool loadScreenConfig(const char* filename, ScreenLayout& layout) {
    // Check for expected JSON structure
    if (!doc["elements"]) {
        Serial.println("[JSON] ERROR: Missing 'elements' array!");
        Serial.println("[JSON] Expected format:");
        Serial.println("[JSON]   { \\\"name\\\": \\\"...\\\", \\\"width\\\": 480, \\\"height\\\": 320, \\\"elements\\\": [...] }");
        return false;
    }

    // Warn about missing optional fields
    if (!doc["width"]) {
        Serial.println("[JSON] Warning: Missing 'width' field (assuming 480)");
    }
    if (!doc["height"]) {
        Serial.println("[JSON] Warning: Missing 'height' field (assuming 320)");
    }

    // Check for old schema (uses 'source' instead of 'data')
    JsonArray elements = doc["elements"].as<JsonArray>();
    if (elements.size() > 0) {
        JsonObject firstElem = elements[0];
        if (firstElem["source"] && !firstElem["data"]) {
            Serial.println("[JSON] ERROR: Old JSON schema detected!");
            Serial.println("[JSON] This file uses 'source' field, but renderer expects 'data'");
            Serial.println("[JSON] Please update JSON files to use 'data' instead of 'source'");
            return false;
        }
    }
    Serial.printf("[JSON] Loading screen config: %s\n", filename);

    // Use StorageManager to load file (auto-fallback SD->SPIFFS)
    String jsonContent = storage.loadFile(filename);

    if (jsonContent.length() == 0) {
        Serial.printf("[JSON] File not found: %s\n", filename);
        return false;
    }

    if (jsonContent.length() > 8192) {
        Serial.printf("[JSON] File too large: %d bytes (max 8192)\n", jsonContent.length());
        return false;
    }

    Serial.printf("[JSON] Loaded %d bytes from %s (%s)\n",
                  jsonContent.length(), filename, storage.getStorageType(filename).c_str());

    // CRITICAL: Yield before JSON parsing (prevents mutex deadlock)
    yield();

    yield();  // Yield before deserialize
    DeserializationError error = deserializeJson(doc, jsonContent);
    yield();  // Yield after deserialize

    if (error) {
        Serial.printf("[JSON] Parse error: %s\n", error.c_str());
        return false;
    }

    // ===== ENHANCED ERROR CHECKING =====
    // NOTE: Removed misplaced validation block that referenced undefined variables (elem, elementIndex)
    // The per-element validation is now performed inside the element parsing loop below.
    // ===== END ERROR CHECKING =====

    // Extract layout info
    strncpy(layout.name, doc["name"] | "Unnamed", sizeof(layout.name) - 1);
    layout.backgroundColor = parseColor(doc["background"] | "0000");
    layout.elementCount = 0;
    layout.isValid = false;

    // Parse elements array
    JsonArray elements = doc["elements"].as<JsonArray>();
    if (!elements) {
        Serial.println("[JSON] No elements array found");
        return false;
    }

    int elementIndex = 0;
    // Iterate as JsonVariantConst then convert to JsonObjectConst to ensure proper typing
    for (JsonVariantConst v : elements) {
        JsonObjectConst obj = v.as<JsonObjectConst>();

        if (elementIndex >= 60) {
            Serial.println("[JSON] Warning: Max 60 elements, ignoring rest");
            break;
        }

        yield();  // Yield during element parsing loop

        // New: validate required "type" field in the correct (loop) scope
        if (!obj["type"]) {
            Serial.printf("[JSON] Warning: Element %d missing 'type' field, skipping\n", elementIndex);
            elementIndex++; // keep index consistent with skipped element count
            continue;
        }

        ScreenElement& se = layout.elements[elementIndex];

        // Parse element properties
        se.type = parseElementType(obj["type"] | "none");
        se.x = obj["x"] | 0;
        se.y = obj["y"] | 0;
        se.w = obj["w"] | 0;
        se.h = obj["h"] | 0;
        se.color = parseColor(obj["color"] | "FFFF");
        se.bgColor = parseColor(obj["bgColor"] | "0000");
        se.textSize = obj["size"] | 2;
        se.decimals = obj["decimals"] | 2;
        se.filled = obj["filled"] | true;
        se.showLabel = obj["showLabel"] | true;
        se.align = parseAlignment(obj["align"] | "left");

        // Copy strings
        strncpy(se.label, obj["label"] | "", sizeof(se.label) - 1);
        //strncpy(se.dataSource, obj["data"] | "", sizeof(se.dataSource) - 1);
        // Parse and validate data source
        const char* rawDataSource = obj["data"] | "";
        char normalizedSource[32];

        if (strlen(rawDataSource) > 0) {
            bool isValid = validateDataSource(rawDataSource, normalizedSource, sizeof(normalizedSource));
            strncpy(se.dataSource, normalizedSource, sizeof(se.dataSource) - 1);

            if (!isValid) {
                Serial.printf("[JSON] Element %d uses unrecognized data source: %s\n", 
                            elementIndex, rawDataSource);
            }
        } else {
            se.dataSource[0] = '\0';
        }
        elementIndex++;
    }

    yield();  // Final yield after parsing complete

    layout.elementCount = elementIndex;
    layout.isValid = true;

    Serial.printf("[JSON] Loaded %d elements from %s\n", elementIndex, layout.name);
    return true;
}

// Initialize default/fallback layouts in case JSON files are missing
void initDefaultLayouts() {
    // Mark all layouts as invalid initially
    monitorLayout.isValid = false;
    alignmentLayout.isValid = false;
    graphLayout.isValid = false;
    networkLayout.isValid = false;

    strcpy(monitorLayout.name, "Monitor (Fallback)");
    strcpy(alignmentLayout.name, "Alignment (Fallback)");
    strcpy(graphLayout.name, "Graph (Fallback)");
    strcpy(networkLayout.name, "Network (Fallback)");

    Serial.println("[JSON] Default layouts initialized (fallback mode)");
}

// ========== DATA ACCESS FUNCTIONS ==========

// Get numeric data value from data source identifier
float getDataValue(const char* dataSource) {
    if (strcmp(dataSource, "posX") == 0) return posX;
    if (strcmp(dataSource, "posY") == 0) return posY;
    if (strcmp(dataSource, "posZ") == 0) return posZ;
    if (strcmp(dataSource, "posA") == 0) return posA;

    if (strcmp(dataSource, "wposX") == 0) return wposX;
    if (strcmp(dataSource, "wposY") == 0) return wposY;
    if (strcmp(dataSource, "wposZ") == 0) return wposZ;
    if (strcmp(dataSource, "wposA") == 0) return wposA;

    if (strcmp(dataSource, "feedRate") == 0) return feedRate;
    if (strcmp(dataSource, "spindleRPM") == 0) return spindleRPM;
    if (strcmp(dataSource, "psuVoltage") == 0) return psuVoltage;
    if (strcmp(dataSource, "fanSpeed") == 0) return fanSpeed;

    if (strcmp(dataSource, "temp0") == 0) return temperatures[0];
    if (strcmp(dataSource, "temp1") == 0) return temperatures[1];
    if (strcmp(dataSource, "temp2") == 0) return temperatures[2];
    if (strcmp(dataSource, "temp3") == 0) return temperatures[3];

    return 0.0f;
}

// Get string data value from data source identifier
String getDataString(const char* dataSource) {
    if (strcmp(dataSource, "machineState") == 0) return machineState;
    if (strcmp(dataSource, "ipAddress") == 0) return WiFi.localIP().toString();
    if (strcmp(dataSource, "ssid") == 0) return WiFi.SSID();
    if (strcmp(dataSource, "deviceName") == 0) return String(cfg.device_name);
    if (strcmp(dataSource, "fluidncIP") == 0) return String(cfg.fluidnc_ip);

    // RTC date/time data sources
    if (rtcAvailable) {
        DateTime now = rtc.now();
        char buffer[32];

        if (strcmp(dataSource, "rtcTime") == 0) {
            // Format: HH:MM:SS
            sprintf(buffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            return String(buffer);
        }
        if (strcmp(dataSource, "rtcTime12") == 0) {
            // Format: HH:MM:SS AM/PM
            int hour12 = now.hour() % 12;
            if (hour12 == 0) hour12 = 12;
            sprintf(buffer, "%02d:%02d:%02d %s", hour12, now.minute(), now.second(),
                    now.hour() >= 12 ? "PM" : "AM");
            return String(buffer);
        }
        if (strcmp(dataSource, "rtcTimeShort") == 0) {
            // Format: HH:MM
            sprintf(buffer, "%02d:%02d", now.hour(), now.minute());
            return String(buffer);
        }
        if (strcmp(dataSource, "rtcDate") == 0) {
            // Format: YYYY-MM-DD
            sprintf(buffer, "%04d-%02d-%02d", now.year(), now.month(), now.day());
            return String(buffer);
        }
        if (strcmp(dataSource, "rtcDateShort") == 0) {
            // Format: MM/DD/YYYY
            sprintf(buffer, "%02d/%02d/%04d", now.month(), now.day(), now.year());
            return String(buffer);
        }
        if (strcmp(dataSource, "rtcDateTime") == 0 || strcmp(dataSource, "dateTime") == 0) {
            // Format: YYYY-MM-DD HH:MM:SS
            sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second());
            return String(buffer);
        }
    } else {
        // RTC not available
        if (strncmp(dataSource, "rtc", 3) == 0) {
            return String("No RTC");
        }
    }

    // Numeric values as strings
    float value = getDataValue(dataSource);
    return String(value, 2);
}

// ========== DRAWING FUNCTIONS ==========

// Draw a single screen element
void drawElement(const ScreenElement& el) {
    switch(el.type) {
        case ELEM_RECT:
            if (el.filled) {
                gfx.fillRect(el.x, el.y, el.w, el.h, el.color);
            } else {
                gfx.drawRect(el.x, el.y, el.w, el.h, el.color);
            }
            break;

        case ELEM_LINE:
            if (el.w > el.h) {
                // Horizontal line
                gfx.drawFastHLine(el.x, el.y, el.w, el.color);
            } else {
                // Vertical line
                gfx.drawFastVLine(el.x, el.y, el.h, el.color);
            }
            break;

        case ELEM_TEXT_STATIC:
            {
                gfx.setTextColor(el.color);
                gfx.setTextSize(el.textSize);

                // Use old rendering if no w/h specified (backward compatibility)
                if (el.w == 0 || el.h == 0) {
                    gfx.setCursor(el.x, el.y);
                    gfx.print(el.label);
                } else {
                    // Use LovyanGFX smooth font rendering
                    selectBestFont(el.textSize);
                    float scale = el.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    switch(el.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(el.label, el.x + el.w / 2, el.y + el.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(el.label, el.x + el.w, el.y + el.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(el.label, el.x, el.y + el.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_TEXT_DYNAMIC:
            {
                gfx.setTextColor(el.color);
                gfx.setTextSize(el.textSize);

                String value = getDataString(el.dataSource);

                // Use old rendering if no w/h specified (backward compatibility)
                if (el.w == 0 || el.h == 0) {
                    gfx.setCursor(el.x, el.y);
                    if (el.showLabel && el.label[0] != '\0') {
                        gfx.print(el.label);
                    }
                    gfx.print(value);
                } else {
                    // Use LovyanGFX smooth font rendering
                    selectBestFont(el.textSize);
                    float scale = el.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    String displayText = "";
                    if (el.showLabel && el.label[0] != '\0') {
                        displayText = String(el.label) + value;
                    } else {
                        displayText = value;
                    }

                    switch(el.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, el.x + el.w / 2, el.y + el.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, el.x + el.w, el.y + el.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, el.x, el.y + el.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_TEMP_VALUE:
            {
                gfx.setTextColor(el.color);
                gfx.setTextSize(el.textSize);

                float temp = getDataValue(el.dataSource);
                if (cfg.use_fahrenheit) {
                    temp = temp * 9.0 / 5.0 + 32.0;
                }

                // Use old rendering if no w/h specified (backward compatibility)
                if (el.w == 0 || el.h == 0) {
                    gfx.setCursor(el.x, el.y);
                    if (el.showLabel && el.label[0] != '\0') {
                        gfx.print(el.label);
                    }
                    gfx.printf("%.*f%c", el.decimals, temp,
                              cfg.use_fahrenheit ? 'F' : 'C');
                } else {
                    // Use LovyanGFX smooth font rendering
                    selectBestFont(el.textSize);
                    float scale = el.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    char tempStr[32];
                    snprintf(tempStr, sizeof(tempStr), "%.*f%c", el.decimals, temp,
                            cfg.use_fahrenheit ? 'F' : 'C');

                    String displayText = "";
                    if (el.showLabel && el.label[0] != '\0') {
                        displayText = String(el.label) + String(tempStr);
                    } else {
                        displayText = String(tempStr);
                    }

                    switch(el.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, el.x + el.w / 2, el.y + el.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, el.x + el.w, el.y + el.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, el.x, el.y + el.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_COORD_VALUE:
            {
                gfx.setTextColor(el.color);
                gfx.setTextSize(el.textSize);

                float value = getDataValue(el.dataSource);
                if (cfg.use_inches) {
                    value = value / 25.4;
                }

                // Use old rendering if no w/h specified (backward compatibility)
                if (el.w == 0 || el.h == 0) {
                    gfx.setCursor(el.x, el.y);
                    if (el.showLabel && el.label[0] != '\0') {
                        gfx.print(el.label);
                    }
                    gfx.printf("%.*f", el.decimals, value);
                } else {
                    // Use LovyanGFX smooth font rendering
                    selectBestFont(el.textSize);
                    float scale = el.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    char coordStr[32];
                    snprintf(coordStr, sizeof(coordStr), "%.*f", el.decimals, value);

                    String displayText = "";
                    if (el.showLabel && el.label[0] != '\0') {
                        displayText = String(el.label) + String(coordStr);
                    } else {
                        displayText = String(coordStr);
                    }

                    switch(el.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, el.x + el.w / 2, el.y + el.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, el.x + el.w, el.y + el.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, el.x, el.y + el.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_STATUS_VALUE:
            {
                gfx.setTextSize(el.textSize);

                // Color-code machine state
                if (strcmp(el.dataSource, "machineState") == 0) {
                    if (machineState == "RUN") {
                        gfx.setTextColor(COLOR_GOOD);
                    } else if (machineState == "ALARM") {
                        gfx.setTextColor(COLOR_WARN);
                    } else {
                        gfx.setTextColor(el.color);
                    }
                } else {
                    gfx.setTextColor(el.color);
                }

                String value = getDataString(el.dataSource);

                // Use old rendering if no w/h specified (backward compatibility)
                if (el.w == 0 || el.h == 0) {
                    gfx.setCursor(el.x, el.y);
                    if (el.showLabel && el.label[0] != '\0') {
                        gfx.print(el.label);
                    }
                    gfx.print(value);
                } else {
                    // Use LovyanGFX smooth font rendering
                    selectBestFont(el.textSize);
                    float scale = el.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    String displayText = "";
                    if (el.showLabel && el.label[0] != '\0') {
                        displayText = String(el.label) + value;
                    } else {
                        displayText = value;
                    }

                    switch(el.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, el.x + el.w / 2, el.y + el.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, el.x + el.w, el.y + el.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, el.x, el.y + el.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_PROGRESS_BAR:
            {
                // Draw outline
                gfx.drawRect(el.x, el.y, el.w, el.h, el.color);

                // Calculate progress (placeholder - would need job tracking)
                int progress = 0;  // 0-100%
                int fillWidth = (el.w - 2) * progress / 100;

                // Draw filled portion
                if (fillWidth > 0) {
                    gfx.fillRect(el.x + 1, el.y + 1,
                               fillWidth, el.h - 2, el.color);
                }
            }
            break;

        case ELEM_GRAPH:
            {
                // Draw temperature graph
                gfx.fillRect(el.x, el.y, el.w, el.h, el.bgColor);
                gfx.drawRect(el.x, el.y, el.w, el.h, el.color);

                if (tempHistory != nullptr && historySize > 0) {
                    float minTemp = 10.0;
                    float maxTemp = 60.0;

                    // Draw temperature line
                    for (int i = 1; i < historySize; i++) {
                        int idx1 = (historyIndex + i - 1) % historySize;
                        int idx2 = (historyIndex + i) % historySize;

                        float temp1 = tempHistory[idx1];
                        float temp2 = tempHistory[idx2];

                        int x1 = el.x + ((i - 1) * el.w / historySize);
                        int y1 = el.y + el.h - ((temp1 - minTemp) / (maxTemp - minTemp) * el.h);
                        int x2 = el.x + (i * el.w / historySize);
                        int y2 = el.y + el.h - ((temp2 - minTemp) / (maxTemp - minTemp) * el.h);

                        y1 = constrain(y1, el.y, el.y + el.h);
                        y2 = constrain(y2, el.y, el.y + el.h);

                        // Color based on temperature (use element color or threshold-based)
                        uint16_t color;
                        if (temp2 > cfg.temp_threshold_high) color = COLOR_WARN;
                        else if (temp2 > cfg.temp_threshold_low) color = COLOR_ORANGE;
                        else color = COLOR_GOOD;

                        gfx.drawLine(x1, y1, x2, y2, color);
                    }

                    // Scale markers
                    gfx.setTextSize(1);
                    gfx.setTextColor(el.color);
                    gfx.setCursor(el.x + 3, el.y + 2);
                    gfx.print("60");
                    gfx.setCursor(el.x + 3, el.y + el.h / 2 - 5);
                    gfx.print("35");
                    gfx.setCursor(el.x + 3, el.y + el.h - 10);
                    gfx.print("10");
                }
            }
            break;

        default:
            break;
    }
}

// Draw entire screen from layout definition
void drawScreenFromLayout(const ScreenLayout& layout) {
    if (!layout.isValid) {
        Serial.println("[JSON] Invalid layout, cannot draw");
        return;
    }

    // Clear screen with background color
    gfx.fillScreen(layout.backgroundColor);

    // Draw all elements
    for (uint8_t i = 0; i < layout.elementCount; i++) {
        drawElement(layout.elements[i]);
    }
}
