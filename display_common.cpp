/**
 * ============================================================================
 * ФАЙЛ: display_common.cpp
 * ОБЩИЕ ФУНКЦИИ ОТРИСОВКИ ДЛЯ ВСЕХ РЕЖИМОВ ДИСПЛЕЯ
 * 
 * ВЕРСИЯ: 1.0
 * 
 * СОДЕРЖИТ:
 * 1. Проверку валидности температур
 * 2. Очистку областей отображения
 * 3. Отрисовку температуры и дельты
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
  tft.fillRect(10, y, maxTempWidth, bigFontHeight, bgColor);
}

// ============================================================================
// ОЧИСТКА ОБЛАСТИ ДЕЛЬТЫ
// ============================================================================
void display_clear_delta_area(int y, const char* deltaStr, uint16_t bgColor) {
  // Временно переключаемся на шрифт дельты для расчёта ширины
  tft.setTextFont(FONT_DELTA);
  
  // Рассчитываем позицию дельты (правый край с отступом 10 пикселей)
  int deltaWidth = tft.textWidth(deltaStr);
  int deltaX = 240 - deltaWidth - 10;  // 240 - ширина экрана
  
  // Рассчитываем вертикальную позицию (снизу области)
  int deltaY = y + (RECT_HEIGHT - deltaFontHeight);
  
  // Корректировка, если дельта выходит за нижнюю границу
  if (deltaY + deltaFontHeight > y + RECT_HEIGHT) {
    deltaY = y + RECT_HEIGHT - deltaFontHeight - 5;
  }
  
  // Затираем область с небольшим запасом по бокам
  tft.fillRect(deltaX - 5, deltaY, deltaWidth + 10, deltaFontHeight, bgColor);
  
  // Возвращаем основной шрифт
  tft.setTextFont(FONT_BIG);
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
  
  // Короткая задержка для стабильности SPI
  vTaskDelay(pdMS_TO_TICKS(1));
}

// ============================================================================
// ОТРИСОВКА ДЕЛЬТЫ
// ============================================================================
void display_draw_delta(int y, float delta, uint16_t textColor, uint16_t bgColor) {
  tft.setTextFont(FONT_DELTA);
  tft.setTextColor(textColor, bgColor);

  // Форматирование дельты со знаком
  char deltaStr[8];
  if (delta < -100.0f || delta > 100.0f) {
    // Защита от нереальных значений
    strcpy(deltaStr, "0.00");
  } else if (delta >= 0) {
    sprintf(deltaStr, "+%.2f", delta);  // Явный знак плюс
  } else {
    sprintf(deltaStr, "%.2f", delta);   // Минус уже в числе
  }

  // Рассчёт позиции (аналогично очистке)
  int deltaWidth = tft.textWidth(deltaStr);
  int deltaX = 240 - deltaWidth - 10;
  int deltaY = y + (RECT_HEIGHT - deltaFontHeight);
  
  if (deltaY + deltaFontHeight > y + RECT_HEIGHT) {
    deltaY = y + RECT_HEIGHT - deltaFontHeight - 5;
  }

  tft.setCursor(deltaX, deltaY);
  tft.print(deltaStr);
  
  // Возвращаем основной шрифт
  tft.setTextFont(FONT_BIG);
  vTaskDelay(pdMS_TO_TICKS(1));
}