#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ УПРАВЛЕНИЯ ДИСПЛЕЕМ
// ============================================================================

/**
 * ЗАДАЧА ДИСПЛЕЯ (ДЛЯ FREERTOS)
 * Основной цикл обновления экрана
 */
void taskDisplay(void* pvParameters);

/**
 * ПОЛНАЯ ПЕРЕРИСОВКА ДИСПЛЕЯ
 * Заливает экран текущим цветом фона и сбрасывает кэш
 */
void performFullDisplayRedraw();

/**
 * ОПРЕДЕЛЕНИЕ ТЕКУЩЕГО ЦВЕТА ФОНА
 * Возвращает цвет в зависимости от режима и состояния
 */
uint16_t getCurrentBackgroundColor();

/**
 * СБРОС СОСТОЯНИЯ ДИСПЛЕЯ ПРИ ПЕРЕКЛЮЧЕНИИ РЕЖИМА
 * @param newMode - новый режим (0 = MODE1, 1 = MODE2)
 */
void resetDisplayState(uint8_t newMode);

#endif // DISPLAY_ENGINE_H