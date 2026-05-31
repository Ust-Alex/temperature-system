/**
 * ============================================================================
 * ФАЙЛ: display_common.cpp
 * ОБЩИЕ ФУНКЦИИ ОТРИСОВКИ ДЛЯ ВСЕХ РЕЖИМОВ ДИСПЛЕЯ
 * 
 * ВЕРСИЯ: 2.0 (ДЕЛЬТА ПОЛНОСТЬЮ УДАЛЕНА)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - Удалены функции display_clear_delta_area() и display_draw_delta()
 * - Удалены все упоминания дельты
 * - Оставлены только функции для работы с температурой
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
// ОЧИСТКА ОБЛАСТИ ТЕМПЕРАТУРЫ
// ============================================================================
void display_clear_temperature_area(int y, uint16_t bgColor) {
  // Затираем прямоугольник под температуру цветом фона
  tft.fillRect(10, y, maxTempWidth, bigFontHeight, bgColor);
}

// ============================================================================
// ОТРИСОВКА ТЕМПЕРАТУРЫ
// ============================================================================
void display_draw_temperature(int y, float temp, uint16_t textColor, uint16_t bgColor) {
  tft.setTextFont(FONT_BIG);

  // Определяем цвет текста для специальных значений
  uint16_t actualTextColor = textColor;
  if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) {
    actualTextColor = COLOR_RED;  // Критическая потеря - красный
  } else if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) {
    actualTextColor = COLOR_YELLOW;  // Потеря стенки - жёлтый
  }

  tft.setTextColor(actualTextColor, bgColor);

  // Вертикальное центрирование в области датчика
  int tempY = y + (RECT_HEIGHT - bigFontHeight) / 2;
  tft.setCursor(10, tempY);

  // Отрисовка с учётом валидности и формата
  if (!display_is_valid_temperature(temp)) {
    tft.print("--.--");  // Для невалидных значений
  } else {
    // Добавляем ведущий ноль для значений 0-9 (например, "08.23")
    if (temp < 10.0f && temp >= 0) {
      tft.printf("0%.2f", temp);
    } else {
      tft.printf("%.2f", temp);
    }
  }
}