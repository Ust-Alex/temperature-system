#ifndef MODE2_LOGIC_H
#define MODE2_LOGIC_H

#include "system_config.h"

// ============================================================================
// ОБЪЯВЛЕНИЯ ФУНКЦИЙ (упрощенные)
// ============================================================================

// Основная функция обновления цветового состояния
void mode2_update_color_state(float currentGuildTemp);

// Функции для установки/получения базовой температуры
void mode2_set_base_temperature(float baseTemp);
float mode2_get_base_temperature();

// Функции таймера MODE2
void mode2_timer_start();
void mode2_timer_stop();
void mode2_timer_reset();
String mode2_timer_get_formatted();
bool mode2_timer_is_active();

// Вспомогательная функция для получения текущего цветового состояния
uint8_t mode2_get_current_color_state();

#endif