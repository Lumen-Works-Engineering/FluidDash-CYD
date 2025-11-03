#ifndef SCREEN_RENDERER_H
#define SCREEN_RENDERER_H

#include <Arduino.h>
#include "config/config.h"

// JSON parsing functions
uint16_t parseColor(const char* hexColor);
ElementType parseElementType(const char* typeStr);
TextAlign parseAlignment(const char* alignStr);

// Screen layout functions
bool loadScreenConfig(const char* filename, ScreenLayout& layout);
void initDefaultLayouts();

// Drawing functions
void drawScreenFromLayout(const ScreenLayout& layout);
void drawElement(const ScreenElement& elem);

// Data access functions
float getDataValue(const char* dataSource);
String getDataString(const char* dataSource);

#endif // SCREEN_RENDERER_H
