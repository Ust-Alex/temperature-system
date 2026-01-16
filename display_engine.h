#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include "system_config.h"
#include "mode2_logic.h"

void performFullDisplayRedraw();
void clearTemperatureArea(int y, uint16_t bgColor);
void clearDeltaArea(int y, const char* deltaStr, uint16_t bgColor);
void drawTemperature(int y, float temp, uint16_t textColor, uint16_t bgColor);
void drawDelta(int y, float delta, uint16_t textColor, uint16_t bgColor);
void updateDisplayMODE1();
void updateDisplayMODE2_Common(uint16_t bgColor, uint16_t textColor);
void updateDisplayMODE2_GREEN();
void updateDisplayMODE2_YELLOW();
void updateDisplayMODE2_RED();
uint16_t getCurrentBackgroundColor();
void resetDisplayState(uint8_t newMode);

#endif