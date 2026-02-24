/**
 * @file menu_drawing.h
 * @brief ЗАГОЛОВОЧНЫЙ ФАЙЛ ГРАФИЧЕСКОГО МОДУЛЯ МЕНЮ
 * 
 * @version 2.0
 * @date 2026
 * 
 * @details Определяет интерфейс для функций отрисовки меню.
 *          Все функции получают параметры и не хранят состояние.
 */

#ifndef MENU_DRAWING_H
#define MENU_DRAWING_H

#include <Arduino.h>
#include <stdint.h>

// ============================================================================
// КОНСТАНТЫ ОТРИСОВКИ
// ============================================================================
#define MENU_INACTIVITY_TIMEOUT 30000  /**< Таймаут неактивности меню (мс) */
#define MENU_START_Y 5                  /**< Начальная Y-координата первого пункта */
#define MENU_ITEM_HEIGHT 40              /**< Высота пункта меню */
#define MENU_ITEM_WIDTH 220               /**< Ширина пункта меню */
#define MENU_ITEM_SPACING 5               /**< Расстояние между пунктами */

// Цвета
#define MENU_BG_COLOR COLOR_BLACK        /**< Цвет фона по умолчанию */
#define MENU_TEXT_COLOR COLOR_WHITE       /**< Цвет текста по умолчанию */
#define MENU_SELECT_BG COLOR_WHITE        /**< Цвет фона выбранного пункта */
#define MENU_SELECT_TEXT COLOR_BLACK      /**< Цвет текста выбранного пункта */

// ============================================================================
// ФУНКЦИИ ПОЛНОЙ ОТРИСОВКИ
// ============================================================================
void drawMenuTop(uint8_t selectedItem);
void drawMenuMode(uint8_t selectedItem, uint8_t selectedMode, bool modeConfirmed);
void drawMenuVolume(uint8_t selectedItem, uint8_t volume);

// ============================================================================
// ФУНКЦИИ ЧАСТИЧНОГО ОБНОВЛЕНИЯ
// ============================================================================
void updateMenuTopSelection(uint8_t oldItem, uint8_t newItem);
void updateMenuModeSelection(uint8_t currentItem, uint8_t currentSelectedMode, bool isModeConfirmed);
void updateMenuVolume(uint8_t volume, uint8_t currentItem);

// ============================================================================
// УПРАВЛЕНИЕ КЭШЕМ
// ============================================================================
void drawing_reset_cache();
void drawing_reset_volume_cache();

#endif // MENU_DRAWING_H