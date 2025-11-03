#ifndef UI_MODES_H
#define UI_MODES_H

// Display mode functions
void drawScreen();
void updateDisplay();

// Mode drawing functions
void drawMonitorMode();
void updateMonitorMode();
void drawAlignmentMode();
void updateAlignmentMode();
void drawGraphMode();
void updateGraphMode();
void drawNetworkMode();
void updateNetworkMode();

// Helper functions
void drawTempGraph(int x, int y, int w, int h);
void handleButton();
void cycleDisplayMode();
void showHoldProgress();

#endif // UI_MODES_H
