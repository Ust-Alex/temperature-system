#ifndef DISPLAY_COMMON_H
#define DISPLAY_COMMON_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// ============================================================================
// ОБЩИЕ ФУНКЦИИ ОТРИСОВКИ
// ============================================================================

/**
 * ПРОВЕРКА ВАЛИДНОСТИ ТЕМПЕРАТУРЫ
 * @param temp - проверяемая температура
 * @return true если температура валидна для отображения
 */
bool display_is_valid_temperature(float temp);

/**
 * ОЧИСТКА ОБЛАСТИ ОТОБРАЖЕНИЯ ТЕМПЕРАТУРЫ
 * Затирает прямоугольник, где выводится температура, цветом фона
 * 
 * @param x - горизонтальная координата области датчика (левая граница)
 * @param y - вертикальная координата области датчика (верхняя граница)
 * @param bgColor - цвет фона для заливки
 * @param font - шрифт, используемый для температуры (FONT_BIG или FONT_DELTA)
 */
void display_clear_temperature_area(int x, int y, uint16_t bgColor, int font);

#endif // DISPLAY_COMMON_H