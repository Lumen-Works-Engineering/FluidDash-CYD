#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Display Modes
enum DisplayMode {
  MODE_MONITOR,
  MODE_ALIGNMENT,
  MODE_GRAPH,
  MODE_NETWORK
};

// Element types for JSON-defined screens
enum ElementType {
    ELEM_NONE = 0,
    ELEM_RECT,              // Filled or outline rectangle
    ELEM_LINE,              // Horizontal or vertical line
    ELEM_TEXT_STATIC,       // Fixed label text
    ELEM_TEXT_DYNAMIC,      // Text from data source
    ELEM_TEMP_VALUE,        // Temperature display (temp0-3)
    ELEM_COORD_VALUE,       // Coordinate display (posX, wposX, etc)
    ELEM_STATUS_VALUE,      // Status text (machineState, feedRate, etc)
    ELEM_PROGRESS_BAR,      // Progress bar (for job completion)
    ELEM_GRAPH              // Mini graph placeholder
};

// Alignment options
enum TextAlign {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

// Screen element definition
struct ScreenElement {
    ElementType type;
    int16_t x, y, w, h;
    uint16_t color;
    uint16_t bgColor;
    uint8_t textSize;
    char label[32];          // For static text or prefix (e.g., "X:")
    char dataSource[32];     // Data source identifier (e.g., "wposX", "temp0")
    uint8_t decimals;        // Decimal places for numeric values
    bool filled;             // For rectangles - filled or outline
    TextAlign align;         // Text alignment
    bool showLabel;          // Show label prefix
};

// Screen layout definition
struct ScreenLayout {
    char name[32];
    uint16_t backgroundColor;
    ScreenElement elements[60];  // Max 60 elements per screen
    uint8_t elementCount;
    bool isValid;
};

// Configuration Structure
struct Config {
  // Network
  char device_name[32];
  char fluidnc_ip[16];
  uint16_t fluidnc_port;
  bool fluidnc_auto_discover;

  // Temperature - User Settings
  float temp_threshold_low;
  float temp_threshold_high;

  // Temperature - Admin Calibration
  float temp_offset_x;
  float temp_offset_yl;
  float temp_offset_yr;
  float temp_offset_z;

  // Fan Control
  uint8_t fan_min_speed;
  uint8_t fan_max_speed_limit;  // Safety limit

  // PSU Monitoring
  float psu_voltage_cal;
  float psu_alert_low;
  float psu_alert_high;

  // Display Settings
  uint8_t brightness;
  DisplayMode default_mode;
  bool show_machine_coords;
  bool show_temp_graph;
  uint8_t coord_decimal_places;  // 2 or 3

  // Graph Settings
  uint16_t graph_timespan_seconds;  // 60 to 3600 (1-60 minutes)
  uint16_t graph_update_interval;    // How often to add point (1-60 seconds)

  // Units
  bool use_fahrenheit;
  bool use_inches;

  // Advanced
  bool enable_logging;
  uint16_t status_update_rate;  // FluidNC polling rate (ms)
};

// Global config instance (extern declaration)
extern Config cfg;

// Screen layouts (extern declarations)
extern ScreenLayout monitorLayout;
extern ScreenLayout alignmentLayout;
extern ScreenLayout graphLayout;
extern ScreenLayout networkLayout;
extern bool layoutsLoaded;

// Function declarations
void initDefaultConfig();
void loadConfig();
void saveConfig();

#endif // CONFIG_H
