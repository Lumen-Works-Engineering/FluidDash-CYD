#include "ui_modes.h"
#include "display.h"
#include "screen_renderer.h"
#include <WiFi.h>
#include <RTClib.h>

// External variables from main.cpp
extern DisplayMode currentMode;
extern float temperatures[4];
extern float peakTemps[4];
extern float psuVoltage;
extern uint8_t fanSpeed;
extern uint16_t fanRPM;
extern bool fluidncConnected;
extern String machineState;
extern float posX, posY, posZ, posA;
extern float wposX, wposY, wposZ, wposA;
extern bool inAPMode;
extern bool rtcAvailable;
extern RTC_DS3231 rtc;
extern float *tempHistory;
extern uint16_t historySize;
extern uint16_t historyIndex;
extern unsigned long buttonPressStart;
extern bool buttonPressed;

// Function prototypes
void enterSetupMode();
const char* getMonthName(int month);
void updateDynamicElements(const ScreenLayout& layout);

// ========== MAIN DISPLAY CONTROL ==========

void drawScreen() {
    switch(currentMode) {
        case MODE_MONITOR:
            // Try JSON layout first, fallback to legacy if not available
            if (monitorLayout.isValid) {
                Serial.println("[JSON] Drawing monitor from JSON layout");
                drawScreenFromLayout(monitorLayout);
            } else {
                Serial.println("[Legacy] Drawing monitor with legacy code");
                drawMonitorMode();
            }
            break;

        case MODE_ALIGNMENT:
            if (alignmentLayout.isValid) {
                drawScreenFromLayout(alignmentLayout);
            } else {
                drawAlignmentMode();
            }
            break;

        case MODE_GRAPH:
            if (graphLayout.isValid) {
                drawScreenFromLayout(graphLayout);
            } else {
                drawGraphMode();
            }
            break;

        case MODE_NETWORK:
            if (networkLayout.isValid) {
                drawScreenFromLayout(networkLayout);
            } else {
                drawNetworkMode();
            }
            break;
    }
}

void updateDisplay() {
    if (currentMode == MODE_MONITOR) {
        // Use JSON dynamic update if available, otherwise legacy
        if (monitorLayout.isValid) {
            updateDynamicElements(monitorLayout);
        } else {
            updateMonitorMode();
        }
    } else if (currentMode == MODE_ALIGNMENT) {
        if (alignmentLayout.isValid) {
            updateDynamicElements(alignmentLayout);
        } else {
            updateAlignmentMode();
        }
    } else if (currentMode == MODE_GRAPH) {
        if (graphLayout.isValid) {
            updateDynamicElements(graphLayout);
        } else {
            updateGraphMode();
        }
    } else if (currentMode == MODE_NETWORK) {
        if (networkLayout.isValid) {
            updateDynamicElements(networkLayout);
        } else {
            updateNetworkMode();
        }
    }
}

// Update only dynamic elements (for efficient screen updates)
void updateDynamicElements(const ScreenLayout& layout) {
    if (!layout.isValid) return;

    for (uint8_t i = 0; i < layout.elementCount; i++) {
        const ScreenElement& elem = layout.elements[i];

        // Only update elements that display dynamic data
        if (elem.type == ELEM_TEXT_DYNAMIC ||
            elem.type == ELEM_TEMP_VALUE ||
            elem.type == ELEM_COORD_VALUE ||
            elem.type == ELEM_STATUS_VALUE ||
            elem.type == ELEM_PROGRESS_BAR) {

            // Clear the element area
            if (elem.w > 0 && elem.h > 0) {
                gfx.fillRect(elem.x, elem.y, elem.w, elem.h, elem.bgColor);
            }

            // Redraw the element
            drawElement(elem);
        }
    }
}

// ========== MONITOR MODE ==========

void drawMonitorMode() {
  gfx.fillScreen(COLOR_BG);

  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(10, 6);
  gfx.print("FluidDash");

  // DateTime in header (right side)
  char buffer[40];
  if (rtcAvailable) {
    DateTime now = rtc.now();
    sprintf(buffer, "%s %02d  %02d:%02d:%02d",
            getMonthName(now.month()), now.day(), now.hour(), now.minute(), now.second());
  } else {
    sprintf(buffer, "No RTC");
  }
  gfx.setCursor(270, 6);
  gfx.print(buffer);

  // Dividers
  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);
  gfx.drawFastHLine(0, 175, SCREEN_WIDTH, COLOR_LINE);
  gfx.drawFastVLine(240, 25, 150, COLOR_LINE);

  // Left section - Driver temperatures
  gfx.setTextSize(1);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 30);
  gfx.print("DRIVERS:");

  const char* labels[] = {"X:", "YL:", "YR:", "Z:"};
  for (int i = 0; i < 4; i++) {
    gfx.setCursor(10, 50 + i * 30);
    gfx.setTextColor(COLOR_TEXT);
    gfx.print(labels[i]);

    // Current temp
    gfx.setTextSize(2);
    gfx.setTextColor(temperatures[i] > cfg.temp_threshold_high ? COLOR_WARN : COLOR_VALUE);
    gfx.setCursor(50, 47 + i * 30);
    sprintf(buffer, "%d%s", (int)temperatures[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);

    // Peak temp (smaller, to the right)
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(140, 52 + i * 30);
    sprintf(buffer, "pk:%d%s", (int)peakTemps[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);

    gfx.setTextSize(1);
  }

  // Status section
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 185);
  gfx.print("STATUS:");

  gfx.setCursor(10, 200);
  gfx.setTextColor(COLOR_LINE);
  sprintf(buffer, "Fan: %d%% (%dRPM)", fanSpeed, fanRPM);
  gfx.print(buffer);

  gfx.setCursor(10, 215);
  sprintf(buffer, "PSU: %.1fV", psuVoltage);
  gfx.print(buffer);

  gfx.setCursor(10, 230);
  if (fluidncConnected) {
    if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
    else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
    else gfx.setTextColor(COLOR_VALUE);
    sprintf(buffer, "FluidNC: %s", machineState.c_str());
  } else {
    gfx.setTextColor(COLOR_WARN);
    sprintf(buffer, "FluidNC: Disconnected");
  }
  gfx.print(buffer);

  // Coordinates
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 250);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "WCS: X:%.3f Y:%.3f Z:%.3f", wposX, wposY, wposZ);
  } else {
    sprintf(buffer, "WCS: X:%.2f Y:%.2f Z:%.2f", wposX, wposY, wposZ);
  }
  gfx.print(buffer);

  gfx.setCursor(10, 265);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "MCS: X:%.3f Y:%.3f Z:%.3f", posX, posY, posZ);
  } else {
    sprintf(buffer, "MCS: X:%.2f Y:%.2f Z:%.2f", posX, posY, posZ);
  }
  gfx.print(buffer);

  // Right section - Temperature graph
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(250, 30);
  gfx.print("TEMP HISTORY");

  if (cfg.show_temp_graph) {
    char graphLabel[40];
    if (cfg.graph_timespan_seconds >= 60) {
      sprintf(graphLabel, "(%d min)", cfg.graph_timespan_seconds / 60);
    } else {
      sprintf(graphLabel, "(%d sec)", cfg.graph_timespan_seconds);
    }
    gfx.setCursor(250, 40);
    gfx.setTextColor(COLOR_LINE);
    gfx.print(graphLabel);

    // Draw the temperature history graph
    drawTempGraph(250, 55, 220, 110);
  }
}

void updateMonitorMode() {
  // LovyanGFX version - only update dynamic parts (clear area, then redraw)
  char buffer[80];

  // Update DateTime in header
  if (rtcAvailable) {
    DateTime now = rtc.now();
    sprintf(buffer, "%s %02d  %02d:%02d:%02d",
            getMonthName(now.month()), now.day(), now.hour(), now.minute(), now.second());
  } else {
    sprintf(buffer, "No RTC");
  }
  gfx.fillRect(270, 0, 210, 25, COLOR_HEADER);
  gfx.setTextSize(2);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(270, 6);
  gfx.print(buffer);

  // Update temperature values and peaks
  for (int i = 0; i < 4; i++) {
    // Clear the temperature display area for this driver
    gfx.fillRect(50, 47 + i * 30, 180, 20, COLOR_BG);

    // Current temp
    gfx.setTextSize(2);
    gfx.setTextColor(temperatures[i] > cfg.temp_threshold_high ? COLOR_WARN : COLOR_VALUE);
    gfx.setCursor(50, 47 + i * 30);
    sprintf(buffer, "%d%s", (int)temperatures[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);

    // Peak temp
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(140, 52 + i * 30);
    sprintf(buffer, "pk:%d%s", (int)peakTemps[i], cfg.use_fahrenheit ? "F" : "C");
    gfx.print(buffer);
  }

  // Update status section
  gfx.setTextSize(1);

  // Fan
  gfx.fillRect(10, 200, 220, 10, COLOR_BG);
  gfx.setTextColor(COLOR_LINE);
  gfx.setCursor(10, 200);
  sprintf(buffer, "Fan: %d%% (%dRPM)", fanSpeed, fanRPM);
  gfx.print(buffer);

  // PSU
  gfx.fillRect(10, 215, 220, 10, COLOR_BG);
  gfx.setCursor(10, 215);
  gfx.setTextColor(COLOR_LINE);
  sprintf(buffer, "PSU: %.1fV", psuVoltage);
  gfx.print(buffer);

  // FluidNC Status
  gfx.fillRect(10, 230, 220, 10, COLOR_BG);
  gfx.setCursor(10, 230);
  if (fluidncConnected) {
    if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
    else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
    else gfx.setTextColor(COLOR_VALUE);
    sprintf(buffer, "FluidNC: %s", machineState.c_str());
  } else {
    gfx.setTextColor(COLOR_WARN);
    sprintf(buffer, "FluidNC: Disconnected");
  }
  gfx.print(buffer);

  // WCS Coordinates
  gfx.fillRect(10, 250, 220, 10, COLOR_BG);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setCursor(10, 250);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "WCS: X:%.3f Y:%.3f Z:%.3f", wposX, wposY, wposZ);
  } else {
    sprintf(buffer, "WCS: X:%.2f Y:%.2f Z:%.2f", wposX, wposY, wposZ);
  }
  gfx.print(buffer);

  // MCS Coordinates
  gfx.fillRect(10, 265, 220, 10, COLOR_BG);
  gfx.setCursor(10, 265);
  if (cfg.coord_decimal_places == 3) {
    sprintf(buffer, "MCS: X:%.3f Y:%.3f Z:%.3f", posX, posY, posZ);
  } else {
    sprintf(buffer, "MCS: X:%.2f Y:%.2f Z:%.2f", posX, posY, posZ);
  }
  gfx.print(buffer);

  // Update temperature graph (if enabled)
  if (cfg.show_temp_graph) {
    drawTempGraph(250, 55, 220, 110);
  }
}

// ========== ALIGNMENT MODE ==========

void drawAlignmentMode() {
  gfx.fillScreen(COLOR_BG);

  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(140, 6);
  gfx.print("ALIGNMENT MODE");

  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);

  // Title
  gfx.setTextSize(2);
  gfx.setTextColor(COLOR_HEADER);
  gfx.setCursor(150, 40);
  gfx.print("WORK POSITION");

  // Detect if 4-axis machine (if A-axis is non-zero or moving)
  bool has4Axes = (posA != 0 || wposA != 0);

  if (has4Axes) {
    // 4-AXIS DISPLAY - Slightly smaller to fit all axes
    gfx.setTextSize(4);
    gfx.setTextColor(COLOR_VALUE);

    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "X:%9.3f");
    } else {
      strcpy(coordFormat, "X:%8.2f");
    }

    gfx.setCursor(40, 75);
    gfx.printf(coordFormat, wposX);

    coordFormat[0] = 'Y';
    gfx.setCursor(40, 120);
    gfx.printf(coordFormat, wposY);

    coordFormat[0] = 'Z';
    gfx.setCursor(40, 165);
    gfx.printf(coordFormat, wposZ);

    coordFormat[0] = 'A';
    gfx.setCursor(40, 210);
    gfx.printf(coordFormat, wposA);

    // Small info footer for 4-axis
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(10, 265);
    gfx.printf("Machine: X:%.1f Y:%.1f Z:%.1f A:%.1f", posX, posY, posZ, posA);
  } else {
    // 3-AXIS DISPLAY - Original large size
    gfx.setTextSize(5);
    gfx.setTextColor(COLOR_VALUE);

    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "X:%9.3f");
    } else {
      strcpy(coordFormat, "X:%8.2f");
    }

    gfx.setCursor(40, 90);
    gfx.printf(coordFormat, wposX);

    coordFormat[0] = 'Y';
    gfx.setCursor(40, 145);
    gfx.printf(coordFormat, wposY);

    coordFormat[0] = 'Z';
    gfx.setCursor(40, 200);
    gfx.printf(coordFormat, wposZ);

    // Small info footer for 3-axis
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(10, 270);
    gfx.printf("Machine: X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  }

  // Status line (same for both)
  gfx.setCursor(10, 285);
  if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
  else gfx.setTextColor(COLOR_VALUE);
  gfx.printf("Status: %s", machineState.c_str());

  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) maxTemp = temperatures[i];
  }

  gfx.setTextColor(maxTemp > cfg.temp_threshold_high ? COLOR_WARN : COLOR_LINE);
  gfx.setCursor(10, 300);
  gfx.printf("Temps:%.0fC  Fan:%d%%  PSU:%.1fV", maxTemp, fanSpeed, psuVoltage);
}

void updateAlignmentMode() {
  // Detect if 4-axis machine
  bool has4Axes = (posA != 0 || wposA != 0);

  if (has4Axes) {
    // 4-AXIS UPDATE
    gfx.setTextSize(4);
    gfx.setTextColor(COLOR_VALUE);

    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "%9.3f");
    } else {
      strcpy(coordFormat, "%8.2f");
    }

    // Update X
    gfx.fillRect(140, 75, 330, 32, COLOR_BG);
    gfx.setCursor(140, 75);
    gfx.printf(coordFormat, wposX);

    // Update Y
    gfx.fillRect(140, 120, 330, 32, COLOR_BG);
    gfx.setCursor(140, 120);
    gfx.printf(coordFormat, wposY);

    // Update Z
    gfx.fillRect(140, 165, 330, 32, COLOR_BG);
    gfx.setCursor(140, 165);
    gfx.printf(coordFormat, wposZ);

    // Update A
    gfx.fillRect(140, 210, 330, 32, COLOR_BG);
    gfx.setCursor(140, 210);
    gfx.printf(coordFormat, wposA);

    // Update footer
    gfx.setTextSize(1);
    gfx.fillRect(90, 265, 390, 40, COLOR_BG);

    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(90, 265);
    gfx.printf("X:%.1f Y:%.1f Z:%.1f A:%.1f", posX, posY, posZ, posA);
  } else {
    // 3-AXIS UPDATE - Original code
    gfx.setTextSize(5);
    gfx.setTextColor(COLOR_VALUE);

    char coordFormat[20];
    if (cfg.coord_decimal_places == 3) {
      strcpy(coordFormat, "%9.3f");
    } else {
      strcpy(coordFormat, "%8.2f");
    }

    gfx.fillRect(150, 90, 320, 38, COLOR_BG);
    gfx.setCursor(150, 90);
    gfx.printf(coordFormat, wposX);

    gfx.fillRect(150, 145, 320, 38, COLOR_BG);
    gfx.setCursor(150, 145);
    gfx.printf(coordFormat, wposY);

    gfx.fillRect(150, 200, 320, 38, COLOR_BG);
    gfx.setCursor(150, 200);
    gfx.printf(coordFormat, wposZ);

    // Update footer
    gfx.setTextSize(1);
    gfx.fillRect(90, 270, 390, 35, COLOR_BG);

    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(90, 270);
    gfx.printf("X:%.1f Y:%.1f Z:%.1f", posX, posY, posZ);
  }

  // Update status (same for both)
  gfx.setCursor(80, 285);
  if (machineState == "RUN") gfx.setTextColor(COLOR_GOOD);
  else if (machineState == "ALARM") gfx.setTextColor(COLOR_WARN);
  else gfx.setTextColor(COLOR_VALUE);
  gfx.printf("%s", machineState.c_str());

  float maxTemp = temperatures[0];
  for (int i = 1; i < 4; i++) {
    if (temperatures[i] > maxTemp) maxTemp = temperatures[i];
  }

  gfx.setTextColor(maxTemp > cfg.temp_threshold_high ? COLOR_WARN : COLOR_LINE);
  gfx.setCursor(90, 300);
  gfx.printf("%.0fC  Fan:%d%%  PSU:%.1fV", maxTemp, fanSpeed, psuVoltage);
}

// ========== GRAPH MODE ==========

void drawGraphMode() {
  gfx.fillScreen(COLOR_BG);

  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(100, 6);
  gfx.print("TEMPERATURE HISTORY");

  char timeLabel[40];
  if (cfg.graph_timespan_seconds >= 60) {
    sprintf(timeLabel, " - %d minutes", cfg.graph_timespan_seconds / 60);
  } else {
    sprintf(timeLabel, " - %d seconds", cfg.graph_timespan_seconds);
  }
  gfx.setTextSize(1);
  gfx.setCursor(330, 10);
  gfx.print(timeLabel);

  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);

  // Full screen graph
  drawTempGraph(20, 40, 440, 270);
}

void updateGraphMode() {
  // Redraw graph
  drawTempGraph(20, 40, 440, 270);
}

// ========== NETWORK MODE ==========

void drawNetworkMode() {
  // Header
  gfx.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(120, 6);
  gfx.print("NETWORK STATUS");

  gfx.drawFastHLine(0, 25, SCREEN_WIDTH, COLOR_LINE);

  gfx.setTextSize(2);
  gfx.setTextColor(COLOR_TEXT);

  if (inAPMode) {
    // AP Mode display
    gfx.setCursor(60, 50);
    gfx.setTextColor(COLOR_WARN);
    gfx.print("WiFi Config Mode Active");

    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(10, 90);
    gfx.print("1. Connect to WiFi network:");

    gfx.setTextSize(2);
    gfx.setTextColor(COLOR_VALUE);
    gfx.setCursor(40, 110);
    gfx.print("FluidDash-Setup");

    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(10, 145);
    gfx.print("2. Open browser and go to:");

    gfx.setTextSize(2);
    gfx.setTextColor(COLOR_VALUE);
    gfx.setCursor(80, 165);
    gfx.print("http://192.168.4.1");

    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(10, 200);
    gfx.print("3. Configure your WiFi settings");

    gfx.setCursor(10, 230);
    gfx.setTextColor(COLOR_LINE);
    gfx.print("Temperature monitoring continues in background");

    // Show to exit AP mode
    gfx.setTextColor(COLOR_ORANGE);
    gfx.setCursor(10, 270);
    gfx.print("Press button briefly to return to monitoring");

  } else {
    // Normal network status display
    if (WiFi.status() == WL_CONNECTED) {
      gfx.setCursor(130, 50);
      gfx.setTextColor(COLOR_GOOD);
      gfx.print("WiFi Connected");

      gfx.setTextSize(1);
      gfx.setTextColor(COLOR_TEXT);

      gfx.setCursor(10, 90);
      gfx.print("SSID:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 90);
      gfx.print(WiFi.SSID());

      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 115);
      gfx.print("IP Address:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 115);
      gfx.print(WiFi.localIP());

      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 140);
      gfx.print("Signal:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 140);
      int rssi = WiFi.RSSI();
      gfx.printf("%d dBm", rssi);

      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 165);
      gfx.print("mDNS:");
      gfx.setTextColor(COLOR_VALUE);
      gfx.setCursor(80, 165);
      gfx.printf("http://%s.local", cfg.device_name);

      if (fluidncConnected) {
        gfx.setTextColor(COLOR_TEXT);
        gfx.setCursor(10, 190);
        gfx.print("FluidNC:");
        gfx.setTextColor(COLOR_GOOD);
        gfx.setCursor(80, 190);
        gfx.print("Connected");
      } else {
        gfx.setTextColor(COLOR_TEXT);
        gfx.setCursor(10, 190);
        gfx.print("FluidNC:");
        gfx.setTextColor(COLOR_WARN);
        gfx.setCursor(80, 190);
        gfx.print("Disconnected");
      }

    } else {
      gfx.setCursor(120, 50);
      gfx.setTextColor(COLOR_WARN);
      gfx.print("WiFi Not Connected");

      gfx.setTextSize(1);
      gfx.setTextColor(COLOR_TEXT);
      gfx.setCursor(10, 100);
      gfx.print("Temperature monitoring active (standalone mode)");

      gfx.setCursor(10, 130);
      gfx.setTextColor(COLOR_ORANGE);
      gfx.print("To configure WiFi:");
    }

    // Instructions for entering AP mode
    gfx.setTextSize(1);
    gfx.setTextColor(COLOR_LINE);
    gfx.setCursor(10, 250);
    gfx.print("Hold button for 10 seconds to enter WiFi");
    gfx.setCursor(10, 265);
    gfx.print("configuration mode");
  }
}

void updateNetworkMode() {
  // Update time in header if needed - but network info is mostly static
  // Could add dynamic signal strength updates here
}

// ========== HELPER FUNCTIONS ==========

void drawTempGraph(int x, int y, int w, int h) {
  gfx.fillRect(x, y, w, h, COLOR_BG);
  gfx.drawRect(x, y, w, h, COLOR_LINE);

  float minTemp = 10.0;
  float maxTemp = 60.0;

  // Draw temperature line
  for (int i = 1; i < historySize; i++) {
    int idx1 = (historyIndex + i - 1) % historySize;
    int idx2 = (historyIndex + i) % historySize;

    float temp1 = tempHistory[idx1];
    float temp2 = tempHistory[idx2];

    int x1 = x + ((i - 1) * w / historySize);
    int y1 = y + h - ((temp1 - minTemp) / (maxTemp - minTemp) * h);
    int x2 = x + (i * w / historySize);
    int y2 = y + h - ((temp2 - minTemp) / (maxTemp - minTemp) * h);

    y1 = constrain(y1, y, y + h);
    y2 = constrain(y2, y, y + h);

    // Color based on temperature
    uint16_t color;
    if (temp2 > cfg.temp_threshold_high) color = COLOR_WARN;
    else if (temp2 > cfg.temp_threshold_low) color = COLOR_ORANGE;
    else color = COLOR_GOOD;

    gfx.drawLine(x1, y1, x2, y2, color);
  }

  // Scale markers
  gfx.setTextSize(1);
  gfx.setTextColor(COLOR_LINE);
  gfx.setCursor(x + 3, y + 2);
  gfx.print("60");
  gfx.setCursor(x + 3, y + h / 2 - 5);
  gfx.print("35");
  gfx.setCursor(x + 3, y + h - 10);
  gfx.print("10");
}

// ========== BUTTON HANDLING ==========

void handleButton() {
  bool currentState = (digitalRead(BTN_MODE) == LOW);

  if (currentState && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = millis();
  }
  else if (!currentState && buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressStart;
    buttonPressed = false;

    if (pressDuration >= 5000) {
      enterSetupMode();
    } else if (pressDuration < 1000) {
      cycleDisplayMode();
    }
  }
  else if (buttonPressed && (millis() - buttonPressStart >= 2000)) {
    showHoldProgress();
  }
}

void cycleDisplayMode() {
  currentMode = (DisplayMode)((currentMode + 1) % 4);  // Now we have 4 modes
  drawScreen();

  // Flash mode name
  gfx.fillRect(180, 140, 120, 40, COLOR_HEADER);
  gfx.setTextColor(COLOR_TEXT);
  gfx.setTextSize(2);
  gfx.setCursor(190, 150);

  switch(currentMode) {
    case MODE_MONITOR: gfx.print("MONITOR"); break;
    case MODE_ALIGNMENT: gfx.print("ALIGNMENT"); break;
    case MODE_GRAPH: gfx.print("GRAPH"); break;
    case MODE_NETWORK: gfx.print("NETWORK"); break;
  }

  delay(800);
  drawScreen();
}

void showHoldProgress() {
  unsigned long elapsed = millis() - buttonPressStart;
  int progress = map(elapsed, 2000, 5000, 0, 100);
  progress = constrain(progress, 0, 100);

  gfx.fillRect(140, 280, 200, 30, COLOR_BG);
  gfx.drawRect(140, 280, 200, 30, COLOR_TEXT);
  gfx.setTextColor(COLOR_WARN);
  gfx.setTextSize(1);
  gfx.setCursor(145, 285);
  gfx.print("Hold for Setup...");

  int barWidth = map(progress, 0, 100, 0, 190);
  gfx.fillRect(145, 295, barWidth, 10, COLOR_WARN);

  gfx.setCursor(145, 307);
  gfx.print(10 - (elapsed / 1000));
  gfx.print(" sec");
}

void enterSetupMode() {
  Serial.println("Entering WiFi configuration AP mode...");

  // Stop any existing WiFi connection
  WiFi.disconnect();
  delay(100);

  // Start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FluidDash-Setup");
  inAPMode = true;

  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());

  // Show AP mode screen
  currentMode = MODE_NETWORK;
  drawScreen();

  Serial.println("WiFi configuration AP active. Device will continue monitoring.");
}

const char* getMonthName(int month) {
  const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return months[month];
}
