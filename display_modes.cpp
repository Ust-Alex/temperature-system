/**
 * ============================================================================
 * ФАЙЛ: display_modes.cpp
 * ФУНКЦИИ ОТРИСОВКИ ДЛЯ РАЗЛИЧНЫХ РЕЖИМОВ РАБОТЫ
 * ВЕРСИЯ: 5.0 (ПЕРЕДАЧА X-КООРДИНАТЫ В ОЧИСТКУ)
 * ============================================================================
 */

#include "display_modes.h"
#include "display_common.h"
#include "globals.h"
#include "system_config.h"

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

    // Очистка области с учётом X-координаты (sensorX[idx])
    display_clear_temperature_area(sensorX[idx], y, BG_COLOR, sensorFont[idx]);

    // Установка шрифта и цвета
    tft.setTextFont(sensorFont[idx]);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    
    // Явные координаты из массива
    tft.setCursor(sensorX[idx], sensorY[idx]);

    // Печать температуры
    if (!display_is_valid_temperature(temp)) {
        tft.print("--.--");
    } else {
        if (temp < 10.0f && temp >= 0) {
            tft.printf("0%.2f", temp);
        } else {
            tft.printf("%.2f", temp);
        }
    }
}

// ============================================================================
// РАБОЧИЙ РЕЖИМ (MODE2)
// ============================================================================

void display_mode2_draw_sensor(int idx, int y, float temp, 
                               uint16_t bgColor, uint16_t textColor) {
    // Очистка области с учётом X-координаты (sensorX[idx])
    display_clear_temperature_area(sensorX[idx], y, bgColor, sensorFont[idx]);

    // Установка шрифта и цвета
    tft.setTextFont(sensorFont[idx]);
    tft.setTextColor(textColor, bgColor);
    
    // Явные координаты из массива
    tft.setCursor(sensorX[idx], sensorY[idx]);

    // Печать температуры
    if (!display_is_valid_temperature(temp)) {
        tft.print("--.--");
    } else {
        if (temp < 10.0f && temp >= 0) {
            tft.printf("0%.2f", temp);
        } else {
            tft.printf("%.2f", temp);
        }
    }
}