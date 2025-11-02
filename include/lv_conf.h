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

// Font settings - enable ONLY fonts used in SquareLine Studio (minimize flash usage)
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 1  // Used for small labels
#define LV_FONT_MONTSERRAT_12 1  // Used for coordinates
#define LV_FONT_MONTSERRAT_14 1  // Default size
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

// Widget support - enable ONLY what's used in the UI (minimize flash usage)
#define LV_USE_ARC        0
#define LV_USE_BAR        0
#define LV_USE_BTN        0
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        0
#define LV_USE_LABEL      1  // Required - we use labels everywhere
#define LV_USE_LINE       0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_CHART      1  // Required - temperature history chart

// Disable all extra features and libs to minimize flash
#define LV_BUILD_EXAMPLES 0
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY 0
#define LV_USE_GRIDNAV 0
#define LV_USE_FRAGMENT 0
#define LV_USE_IMGFONT 0
#define LV_USE_MSG 0

// Enable base widgets that extra widgets depend on (to prevent errors)
// Even though we disabled the extra widgets, their headers are included
// and they check for these dependencies
#undef LV_USE_IMG
#define LV_USE_IMG 1
#undef LV_USE_BTNMATRIX
#define LV_USE_BTNMATRIX 1
#undef LV_USE_TEXTAREA
#define LV_USE_TEXTAREA 1
#undef LV_USE_ARC
#define LV_USE_ARC 1

// Explicitly disable all LVGL extra widgets
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

// Theme
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1  // Matches your dark UI

// Animations
#define LV_USE_ANIMATION 1

// Refresh period in milliseconds
#define LV_DISP_DEF_REFR_PERIOD 30

#endif // LV_CONF_H