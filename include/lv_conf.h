// include/lv_conf.h
#ifndef LV_CONF_H
#define LV_CONF_H

// Color depth - matches ST7796 16-bit color
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// Memory settings
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (32 * 1024U)  // 32KB - reduced to save RAM

// Display settings - match your display
#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 320
#define LV_DPI_DEF 130

// Performance monitoring (disable in production)
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 1

// Logging
#define LV_USE_LOG 1

#if LV_USE_LOG
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#endif

// Font settings - enable fonts you'll use in SquareLine Studio
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

// Widget support - enable what you need
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1
#define LV_USE_CHART      1  // Important for temperature graphs

// Theme
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1  // Matches your dark UI

// Animations
#define LV_USE_ANIMATION 1

// Refresh period in milliseconds
#define LV_DISP_DEF_REFR_PERIOD 30

#endif // LV_CONF_H