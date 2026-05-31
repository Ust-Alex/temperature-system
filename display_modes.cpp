/**
 * ============================================================================
 * ФАЙЛ: display_modes.cpp
 * ФУНКЦИИ ОТРИСОВКИ ДЛЯ РАЗЛИЧНЫХ РЕЖИМОВ РАБОТЫ
 * 
 * ВЕРСИЯ: 3.0 (ДЕЛЬТА ПОЛНОСТЬЮ УДАЛЕНА)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - Удалены все параметры delta из функций отрисовки
 * - Удалены вызовы display_clear_delta_area() и display_draw_delta()
 * - Функции теперь отображают только температуру
 * ============================================================================
 */

#include "display_modes.h"
#include "display_common.h"
#include "globals.h"
#include "system_config.h"

// Внешние переменные
extern TFT_eSPI tft;
extern float timeRefTemp;
extern uint32_t timeStartMs;
extern bool timeIsCounting;

// ============================================================================
// РЕЖИМ СТАБИЛИЗАЦИИ (MODE1)
// ============================================================================

void display_mode1_draw_time() {
  const uint16_t BG_COLOR = COLOR_BLUE;
  const uint16_t TEXT_COLOR = COLOR_WHITE;

  // Формируем строку времени
  String currentTimeString = "00:00";
  if (timeIsCounting) {
    uint32_t elapsedMillis = millis() - timeStartMs;
    uint32_t elapsedMinutes = elapsedMillis / 60000UL;
    uint8_t hours = elapsedMinutes / 60;
    uint8_t minutes = elapsedMinutes % 60;
    
    char timeBuffer[6];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hours, minutes);
    currentTimeString = String(timeBuffer);
  }

  // Отрисовка времени в правом верхнем углу
  tft.fillRect(170, 0, 70, 30, BG_COLOR);
  tft.setTextFont(FONT_DELTA);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setCursor(170, 5);
  tft.print(currentTimeString);
  tft.setTextFont(FONT_BIG);
}

void display_mode1_draw_sensor(int idx, int y, float temp) {
  const uint16_t BG_COLOR = COLOR_BLUE;
  const uint16_t TEXT_COLOR = COLOR_WHITE;

  // Очистка области температуры
  display_clear_temperature_area(y, BG_COLOR);

  // Отрисовка температуры
  display_draw_temperature(y, temp, TEXT_COLOR, BG_COLOR);
}

// ============================================================================
// РАБОЧИЙ РЕЖИМ (MODE2)
// ============================================================================

void display_mode2_draw_sensor(int idx, int y, float temp, 
                               uint16_t bgColor, uint16_t textColor) {
  // Очистка области температуры
  display_clear_temperature_area(y, bgColor);

  // Отрисовка температуры
  display_draw_temperature(y, temp, textColor, bgColor);
}