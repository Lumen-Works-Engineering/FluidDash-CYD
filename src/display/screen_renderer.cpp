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

// ========== JSON PARSING FUNCTIONS ==========

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

// Load screen configuration from JSON file
bool loadScreenConfig(const char* filename, ScreenLayout& layout) {
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

    // Parse JSON - use heap allocation to avoid stack issues
    JsonDocument doc;

    yield();  // Yield before deserialize
    DeserializationError error = deserializeJson(doc, jsonContent);
    yield();  // Yield after deserialize

    if (error) {
        Serial.printf("[JSON] Parse error: %s\n", error.c_str());
        return false;
    }

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
    for (JsonObject elem : elements) {
        if (elementIndex >= 60) {
            Serial.println("[JSON] Warning: Max 60 elements, ignoring rest");
            break;
        }

        yield();  // Yield during element parsing loop

        ScreenElement& se = layout.elements[elementIndex];

        // Parse element properties
        se.type = parseElementType(elem["type"] | "none");
        se.x = elem["x"] | 0;
        se.y = elem["y"] | 0;
        se.w = elem["w"] | 0;
        se.h = elem["h"] | 0;
        se.color = parseColor(elem["color"] | "FFFF");
        se.bgColor = parseColor(elem["bgColor"] | "0000");
        se.textSize = elem["size"] | 2;
        se.decimals = elem["decimals"] | 2;
        se.filled = elem["filled"] | true;
        se.showLabel = elem["showLabel"] | true;
        se.align = parseAlignment(elem["align"] | "left");

        // Copy strings
        strncpy(se.label, elem["label"] | "", sizeof(se.label) - 1);
        strncpy(se.dataSource, elem["data"] | "", sizeof(se.dataSource) - 1);

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
        if (strcmp(dataSource, "rtcDateTime") == 0) {
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
void drawElement(const ScreenElement& elem) {
    switch(elem.type) {
        case ELEM_RECT:
            if (elem.filled) {
                gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.color);
            } else {
                gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);
            }
            break;

        case ELEM_LINE:
            if (elem.w > elem.h) {
                // Horizontal line
                gfx.drawFastHLine(elem.x, elem.y, elem.w, elem.color);
            } else {
                // Vertical line
                gfx.drawFastVLine(elem.x, elem.y, elem.h, elem.color);
            }
            break;

        case ELEM_TEXT_STATIC:
            {
                gfx.setTextColor(elem.color);
                gfx.setTextSize(elem.textSize);

                // Use old rendering if no w/h specified (backward compatibility)
                if (elem.w == 0 || elem.h == 0) {
                    gfx.setCursor(elem.x, elem.y);
                    gfx.print(elem.label);
                } else {
                    // Use LovyanGFX smooth font rendering
                    gfx.setFont(&fonts::Font2);
                    float scale = elem.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    switch(elem.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(elem.label, elem.x + elem.w / 2, elem.y + elem.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(elem.label, elem.x + elem.w, elem.y + elem.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(elem.label, elem.x, elem.y + elem.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_TEXT_DYNAMIC:
            {
                gfx.setTextColor(elem.color);
                gfx.setTextSize(elem.textSize);

                String value = getDataString(elem.dataSource);

                // Use old rendering if no w/h specified (backward compatibility)
                if (elem.w == 0 || elem.h == 0) {
                    gfx.setCursor(elem.x, elem.y);
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        gfx.print(elem.label);
                    }
                    gfx.print(value);
                } else {
                    // Use LovyanGFX smooth font rendering
                    gfx.setFont(&fonts::Font2);
                    float scale = elem.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    String displayText = "";
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        displayText = String(elem.label) + value;
                    } else {
                        displayText = value;
                    }

                    switch(elem.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, elem.x + elem.w / 2, elem.y + elem.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, elem.x + elem.w, elem.y + elem.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, elem.x, elem.y + elem.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_TEMP_VALUE:
            {
                gfx.setTextColor(elem.color);
                gfx.setTextSize(elem.textSize);

                float temp = getDataValue(elem.dataSource);
                if (cfg.use_fahrenheit) {
                    temp = temp * 9.0 / 5.0 + 32.0;
                }

                // Use old rendering if no w/h specified (backward compatibility)
                if (elem.w == 0 || elem.h == 0) {
                    gfx.setCursor(elem.x, elem.y);
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        gfx.print(elem.label);
                    }
                    gfx.printf("%.*f%c", elem.decimals, temp,
                              cfg.use_fahrenheit ? 'F' : 'C');
                } else {
                    // Use LovyanGFX smooth font rendering
                    gfx.setFont(&fonts::Font2);
                    float scale = elem.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    char tempStr[32];
                    snprintf(tempStr, sizeof(tempStr), "%.*f%c", elem.decimals, temp,
                            cfg.use_fahrenheit ? 'F' : 'C');

                    String displayText = "";
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        displayText = String(elem.label) + String(tempStr);
                    } else {
                        displayText = String(tempStr);
                    }

                    switch(elem.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, elem.x + elem.w / 2, elem.y + elem.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, elem.x + elem.w, elem.y + elem.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, elem.x, elem.y + elem.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_COORD_VALUE:
            {
                gfx.setTextColor(elem.color);
                gfx.setTextSize(elem.textSize);

                float value = getDataValue(elem.dataSource);
                if (cfg.use_inches) {
                    value = value / 25.4;
                }

                // Use old rendering if no w/h specified (backward compatibility)
                if (elem.w == 0 || elem.h == 0) {
                    gfx.setCursor(elem.x, elem.y);
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        gfx.print(elem.label);
                    }
                    gfx.printf("%.*f", elem.decimals, value);
                } else {
                    // Use LovyanGFX smooth font rendering
                    gfx.setFont(&fonts::Font2);
                    float scale = elem.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    char coordStr[32];
                    snprintf(coordStr, sizeof(coordStr), "%.*f", elem.decimals, value);

                    String displayText = "";
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        displayText = String(elem.label) + String(coordStr);
                    } else {
                        displayText = String(coordStr);
                    }

                    switch(elem.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, elem.x + elem.w / 2, elem.y + elem.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, elem.x + elem.w, elem.y + elem.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, elem.x, elem.y + elem.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_STATUS_VALUE:
            {
                gfx.setTextSize(elem.textSize);

                // Color-code machine state
                if (strcmp(elem.dataSource, "machineState") == 0) {
                    if (machineState == "RUN") {
                        gfx.setTextColor(COLOR_GOOD);
                    } else if (machineState == "ALARM") {
                        gfx.setTextColor(COLOR_WARN);
                    } else {
                        gfx.setTextColor(elem.color);
                    }
                } else {
                    gfx.setTextColor(elem.color);
                }

                String value = getDataString(elem.dataSource);

                // Use old rendering if no w/h specified (backward compatibility)
                if (elem.w == 0 || elem.h == 0) {
                    gfx.setCursor(elem.x, elem.y);
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        gfx.print(elem.label);
                    }
                    gfx.print(value);
                } else {
                    // Use LovyanGFX smooth font rendering
                    gfx.setFont(&fonts::Font2);
                    float scale = elem.textSize * 1.0f;
                    gfx.setTextSize(scale, scale);

                    String displayText = "";
                    if (elem.showLabel && strlen(elem.label) > 0) {
                        displayText = String(elem.label) + value;
                    } else {
                        displayText = value;
                    }

                    switch(elem.align) {
                        case ALIGN_CENTER:
                            gfx.setTextDatum(textdatum_t::middle_center);
                            gfx.drawString(displayText, elem.x + elem.w / 2, elem.y + elem.h / 2);
                            break;
                        case ALIGN_RIGHT:
                            gfx.setTextDatum(textdatum_t::middle_right);
                            gfx.drawString(displayText, elem.x + elem.w, elem.y + elem.h / 2);
                            break;
                        default:  // ALIGN_LEFT
                            gfx.setTextDatum(textdatum_t::middle_left);
                            gfx.drawString(displayText, elem.x, elem.y + elem.h / 2);
                            break;
                    }
                }
            }
            break;

        case ELEM_PROGRESS_BAR:
            {
                // Draw outline
                gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);

                // Calculate progress (placeholder - would need job tracking)
                int progress = 0;  // 0-100%
                int fillWidth = (elem.w - 2) * progress / 100;

                // Draw filled portion
                if (fillWidth > 0) {
                    gfx.fillRect(elem.x + 1, elem.y + 1,
                               fillWidth, elem.h - 2, elem.color);
                }
            }
            break;

        case ELEM_GRAPH:
            {
                // Draw temperature graph
                gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.bgColor);
                gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);

                if (tempHistory != nullptr && historySize > 0) {
                    float minTemp = 10.0;
                    float maxTemp = 60.0;

                    // Draw temperature line
                    for (int i = 1; i < historySize; i++) {
                        int idx1 = (historyIndex + i - 1) % historySize;
                        int idx2 = (historyIndex + i) % historySize;

                        float temp1 = tempHistory[idx1];
                        float temp2 = tempHistory[idx2];

                        int x1 = elem.x + ((i - 1) * elem.w / historySize);
                        int y1 = elem.y + elem.h - ((temp1 - minTemp) / (maxTemp - minTemp) * elem.h);
                        int x2 = elem.x + (i * elem.w / historySize);
                        int y2 = elem.y + elem.h - ((temp2 - minTemp) / (maxTemp - minTemp) * elem.h);

                        y1 = constrain(y1, elem.y, elem.y + elem.h);
                        y2 = constrain(y2, elem.y, elem.y + elem.h);

                        // Color based on temperature (use element color or threshold-based)
                        uint16_t color;
                        if (temp2 > cfg.temp_threshold_high) color = COLOR_WARN;
                        else if (temp2 > cfg.temp_threshold_low) color = COLOR_ORANGE;
                        else color = COLOR_GOOD;

                        gfx.drawLine(x1, y1, x2, y2, color);
                    }

                    // Scale markers
                    gfx.setTextSize(1);
                    gfx.setTextColor(elem.color);
                    gfx.setCursor(elem.x + 3, elem.y + 2);
                    gfx.print("60");
                    gfx.setCursor(elem.x + 3, elem.y + elem.h / 2 - 5);
                    gfx.print("35");
                    gfx.setCursor(elem.x + 3, elem.y + elem.h - 10);
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
