/**
 * ============================================================================
 * ФАЙЛ: display_common.cpp
 * ОБЩИЕ ФУНКЦИИ ОТРИСОВКИ ДЛЯ ВСЕХ РЕЖИМОВ ДИСПЛЕЯ
 * ВЕРСИЯ: 4.0 (ОЧИСТКА С УЧЁТОМ X-КООРДИНАТЫ)
 * ============================================================================
 */

#include "display_common.h"
#include "globals.h"
#include "system_config.h"

extern TFT_eSPI tft;

// ============================================================================
// ПРОВЕРКА ВАЛИДНОСТИ ТЕМПЕРАТУРЫ
// ============================================================================
bool display_is_valid_temperature(float temp) {
    // Проверка на служебные значения
    if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) return false;
    if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) return false;
    if (temp <= TEMP_NO_DATA + 1.0f && temp >= TEMP_NO_DATA - 1.0f) return false;
    
    // Проверка физического диапазона DS18B20
    if (temp < -55.0f || temp > 125.0f) return false;
    
    return true;
}

// ============================================================================
// ОЧИСТКА ОБЛАСТИ ТЕМПЕРАТУРЫ (С УЧЁТОМ X И Y)
// ============================================================================
void display_clear_temperature_area(int x, int y, uint16_t bgColor, int font) {
    if (font == FONT_BIG) {
        tft.fillRect(x, y, bigTempWidthClear, bigFontHeightClear, bgColor);
    } else {  // FONT_DELTA
        tft.fillRect(x, y, deltaTempWidthClear, deltaFontHeightClear, bgColor);
    }
}