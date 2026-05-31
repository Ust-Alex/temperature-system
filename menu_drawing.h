/**
 * @file menu_drawing.h
 * @brief ЗАГОЛОВОЧНЫЙ ФАЙЛ ГРАФИЧЕСКОГО МОДУЛЯ МЕНЮ
 * @version 2.1
 */

#ifndef MENU_DRAWING_H
#define MENU_DRAWING_H

#include <Arduino.h>
#include <stdint.h>

#define MENU_INACTIVITY_TIMEOUT 30000
#define MENU_START_Y 5
#define MENU_ITEM_HEIGHT 40
#define MENU_ITEM_WIDTH 220
#define MENU_ITEM_SPACING 5

#define MENU_BG_COLOR COLOR_BLACK
#define MENU_TEXT_COLOR COLOR_WHITE
#define MENU_SELECT_BG COLOR_WHITE
#define MENU_SELECT_TEXT COLOR_BLACK

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ
// ============================================================================
void drawMenuTop(uint8_t selectedItem);
void drawMenuMode(uint8_t selectedItem, uint8_t selectedMode, bool modeConfirmed);
void drawMenuVolume(uint8_t selectedItem, uint8_t volume);

void updateMenuTopSelection(uint8_t oldItem, uint8_t newItem);
void updateMenuModeSelection(uint8_t currentItem, uint8_t currentSelectedMode, bool isModeConfirmed);
void updateMenuVolume(uint8_t volume, uint8_t currentItem);

void drawing_reset_cache();
void drawing_reset_volume_cache();

// ============================================================================
// ФУНКЦИИ ДЛЯ КАЛИБРОВКИ
// ============================================================================
void drawMenuCalib(uint8_t selectedItem);
void updateMenuCalibSelection(uint8_t oldItem, uint8_t newItem);
void drawCalibStatus();
void showMessage(const char* msg, uint16_t delayMs);

// ============================================================================
// ФУНКЦИЯ ДЛЯ ЭКРАНА WI-FI
// ============================================================================
void drawWiFiInfoScreen();

#endif