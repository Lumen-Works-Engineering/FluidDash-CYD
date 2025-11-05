#include "screen_renderer.h"
#include "display.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "../webserver/sd_mutex.h"

// External variables from main.cpp (needed for data access)
extern bool sdCardAvailable;
extern float temperatures[4];
extern float posX, posY, posZ, posA;
extern float wposX, wposY, wposZ, wposA;
extern int feedRate;
extern int spindleRPM;
extern float psuVoltage;
extern uint8_t fanSpeed;
extern String machineState;

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
    if (!sdCardAvailable) {
        Serial.printf("[JSON] SD card not available, cannot load %s\n", filename);
        return false;
    }

    Serial.printf("[JSON] Loading screen config: %s\n", filename);

    // EXPLICIT NULL check for mutex
    if (g_sdCardMutex == NULL) {
        Serial.println("[JSON/loadScreenConfig] CRASH PREVENTED: Mutex is NULL!");
        return false;
    }

    // EXPLICIT lock with timeout
    Serial.printf("[JSON/loadScreenConfig] Attempting to lock mutex at 0x%p\n", g_sdCardMutex);
    BaseType_t lockResult = xSemaphoreTake(g_sdCardMutex, pdMS_TO_TICKS(5000));
    if (lockResult != pdTRUE) {
        Serial.println("[JSON/loadScreenConfig] Failed to acquire lock (timeout)");
        return false;
    }
    Serial.println("[JSON/loadScreenConfig] ✓ Lock acquired");

    // Open file
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        BaseType_t unlockResult = xSemaphoreGive(g_sdCardMutex);
        Serial.printf("[JSON/loadScreenConfig] ✓ Unlocked (result=%d)\n", unlockResult);
        Serial.printf("[JSON] Failed to open %s\n", filename);
        return false;
    }

    // Read file content
    size_t fileSize = file.size();
    if (fileSize > 8192) {
        Serial.printf("[JSON] File too large: %d bytes (max 8192)\n", fileSize);
        file.close();
        BaseType_t unlockResult = xSemaphoreGive(g_sdCardMutex);
        Serial.printf("[JSON/loadScreenConfig] ✓ Unlocked (result=%d)\n", unlockResult);
        return false;
    }

    // Allocate buffer
    char* jsonBuffer = (char*)malloc(fileSize + 1);
    if (!jsonBuffer) {
        Serial.println("[JSON] Failed to allocate memory");
        file.close();
        BaseType_t unlockResult = xSemaphoreGive(g_sdCardMutex);
        Serial.printf("[JSON/loadScreenConfig] ✓ Unlocked (result=%d)\n", unlockResult);
        return false;
    }

    // Read file
    size_t bytesRead = file.readBytes(jsonBuffer, fileSize);
    jsonBuffer[bytesRead] = '\0';
    file.close();

    // EXPLICIT unlock - file is closed, data is in memory
    BaseType_t unlockResult = xSemaphoreGive(g_sdCardMutex);
    Serial.printf("[JSON/loadScreenConfig] ✓ Unlocked (result=%d)\n", unlockResult);

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonBuffer);
    free(jsonBuffer);

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
            gfx.setTextSize(elem.textSize);
            gfx.setTextColor(elem.color);
            gfx.setCursor(elem.x, elem.y);
            gfx.print(elem.label);
            break;

        case ELEM_TEXT_DYNAMIC:
            {
                gfx.setTextSize(elem.textSize);
                gfx.setTextColor(elem.color);
                gfx.setCursor(elem.x, elem.y);

                String value = getDataString(elem.dataSource);
                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                gfx.print(value);
            }
            break;

        case ELEM_TEMP_VALUE:
            {
                gfx.setTextSize(elem.textSize);
                gfx.setTextColor(elem.color);
                gfx.setCursor(elem.x, elem.y);

                float temp = getDataValue(elem.dataSource);
                if (cfg.use_fahrenheit) {
                    temp = temp * 9.0 / 5.0 + 32.0;
                }

                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                gfx.printf("%.*f%c", elem.decimals, temp,
                          cfg.use_fahrenheit ? 'F' : 'C');
            }
            break;

        case ELEM_COORD_VALUE:
            {
                gfx.setTextSize(elem.textSize);
                gfx.setTextColor(elem.color);
                gfx.setCursor(elem.x, elem.y);

                float value = getDataValue(elem.dataSource);
                if (cfg.use_inches) {
                    value = value / 25.4;
                }

                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }
                gfx.printf("%.*f", elem.decimals, value);
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

                gfx.setCursor(elem.x, elem.y);

                if (elem.showLabel && strlen(elem.label) > 0) {
                    gfx.print(elem.label);
                }

                String value = getDataString(elem.dataSource);
                gfx.print(value);
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
            // Placeholder for mini-graph rendering
            gfx.drawRect(elem.x, elem.y, elem.w, elem.h, elem.color);
            gfx.setTextSize(1);
            gfx.setTextColor(elem.color);
            gfx.setCursor(elem.x + 5, elem.y + 5);
            gfx.print("GRAPH");
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
