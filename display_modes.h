#ifndef DISPLAY_MODES_H
#define DISPLAY_MODES_H

#include <Arduino.h>

// ============================================================================
// РЕЖИМ СТАБИЛИЗАЦИИ (MODE1)
// ============================================================================
void display_mode1_update();              // Основная функция обновления MODE1
void display_mode1_draw_time();           // Отрисовка таймера
void display_mode1_draw_sensor(int idx, int y, float temp, float delta); // Отрисовка одного датчика

// ============================================================================
// РАБОЧИЙ РЕЖИМ (MODE2)
// ============================================================================
void display_mode2_update();              // Основная функция обновления MODE2
void display_mode2_set_colors();          // Установка цветов в зависимости от состояния
void display_mode2_draw_sensor(int idx, int y, float temp, float delta); // Отрисовка одного датчика

// ============================================================================
// ОБЩИЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (могут использоваться обоими режимами)
// ============================================================================
void display_draw_temperature(int y, float temp, uint16_t textColor, uint16_t bgColor);
void display_draw_delta(int y, float delta, uint16_t textColor, uint16_t bgColor);
void display_clear_temperature_area(int y, uint16_t bgColor);
void display_clear_delta_area(int y, const char* deltaStr, uint16_t bgColor);
bool display_is_valid_temperature(float temp);

#endif