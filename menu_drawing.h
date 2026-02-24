/**
 * @file menu_drawing.h
 * @brief ЗАГОЛОВОЧНЫЙ ФАЙЛ ГРАФИЧЕСКОГО МОДУЛЯ МЕНЮ
 * 
 * @version 2.0 (Финальная, с правильной передачей параметров)
 */

#ifndef MENU_DRAWING_H
#define MENU_DRAWING_H

#include <Arduino.h>
#include <stdint.h>

// ============================================================================
// КОНСТАНТЫ ОТРИСОВКИ
// ============================================================================
#define MENU_INACTIVITY_TIMEOUT 30000
#define MENU_START_Y 5
#define MENU_ITEM_HEIGHT 40
#define MENU_ITEM_WIDTH 220
#define MENU_ITEM_SPACING 5

// Цвета
#define MENU_BG_COLOR COLOR_BLACK
#define MENU_TEXT_COLOR COLOR_WHITE
#define MENU_SELECT_BG COLOR_WHITE
#define MENU_SELECT_TEXT COLOR_BLACK

// ============================================================================
// ФУНКЦИИ ПОЛНОЙ ОТРИСОВКИ (ВСЕГДА ПОЛУЧАЮТ ПАРАМЕТРЫ)
// ============================================================================

/**
 * @brief Полная отрисовка верхнего меню
 * @param selectedItem Какой пункт выделен (0-3)
 */
void drawMenuTop(uint8_t selectedItem);

/**
 * @brief Полная отрисовка экрана выбора режима
 * @param selectedItem Какой пункт выделен (0-3)
 * @param selectedMode Какой режим выбран (0=MODE1, 1=MODE2)
 * @param modeConfirmed Подтверждён ли выбор
 */
void drawMenuMode(uint8_t selectedItem, uint8_t selectedMode, bool modeConfirmed);

/**
 * @brief Полная отрисовка экрана громкости
 * @param selectedItem Какой пункт выделен (0-3)
 * @param volume Текущее значение громкости
 */
void drawMenuVolume(uint8_t selectedItem, uint8_t volume);

// ============================================================================
// ФУНКЦИИ ЧАСТИЧНОГО ОБНОВЛЕНИЯ
// ============================================================================

/**
 * @brief Обновление выделения в верхнем меню
 * @param oldItem Старый выделенный пункт
 * @param newItem Новый выделенный пункт
 */
void updateMenuTopSelection(uint8_t oldItem, uint8_t newItem);

/**
 * @brief Обновление экрана выбора режима (курсор и подсветка)
 * @param currentItem Текущий выделенный пункт
 * @param currentSelectedMode Текущий выбранный режим
 * @param isModeConfirmed Подтверждён ли режим
 */
void updateMenuModeSelection(uint8_t currentItem, uint8_t currentSelectedMode, bool isModeConfirmed);

/**
 * @brief Обновление экрана громкости
 * @param volume Текущая громкость
 * @param currentItem Текущий выделенный пункт
 */
void updateMenuVolume(uint8_t volume, uint8_t currentItem);

// ============================================================================
// СБРОС КЭША (ВЫЗЫВАТЬ ПРИ ПОЛНОЙ ОЧИСТКЕ ЭКРАНА)
// ============================================================================
void drawing_reset_cache();
void drawing_reset_volume_cache();

#endif // MENU_DRAWING_H