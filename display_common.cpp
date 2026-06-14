/**
 * ============================================================================
 * ФАЙЛ: display_common.cpp
 * ОБЩИЕ ФУНКЦИИ ОТРИСОВКИ ДЛЯ ВСЕХ РЕЖИМОВ ДИСПЛЕЯ
 * 
 * ВЕРСИЯ: 3.0 (ОЧИСТКА ОБЛАСТИ С УЧЁТОМ ШРИФТА)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - display_clear_temperature_area() использует предрассчитанные размеры
 * - Функция display_draw_temperature() УДАЛЕНА (отрисовка теперь в display_modes.cpp)
 * ============================================================================
 */

#include "display_common.h"
#include "globals.h"
#include "system_config.h"

// Внешние объекты для работы с дисплеем
extern TFT_eSPI tft;

// ============================================================================
// ПРОВЕРКА ВАЛИДНОСТИ ТЕМПЕРАТУРЫ
// ============================================================================
bool display_is_valid_temperature(float temp) {
  // Проверка на служебные значения (потеря датчика, ошибка)
  if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) return false;
  if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) return false;
  if (temp <= TEMP_NO_DATA + 1.0f && temp >= TEMP_NO_DATA - 1.0f) return false;

  // Проверка физического диапазона DS18B20
  if (temp < -55.0f || temp > 125.0f) return false;

  return true;
}

// ============================================================================
// ОЧИСТКА ОБЛАСТИ ТЕМПЕРАТУРЫ (С УЧЁТОМ ШРИФТА)
// ============================================================================
void display_clear_temperature_area(int y, uint16_t bgColor, int font) {
  if (font == FONT_BIG) {
    tft.fillRect(10, y, bigTempWidthClear, bigFontHeightClear, bgColor);
  } else {  // FONT_DELTA
    tft.fillRect(10, y, deltaTempWidthClear, deltaFontHeightClear, bgColor);
  }
}