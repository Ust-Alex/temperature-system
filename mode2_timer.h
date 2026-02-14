#ifndef MODE2_TIMER_H
#define MODE2_TIMER_H

#include <Arduino.h>

// ============================================================================
// УПРАВЛЕНИЕ ТАЙМЕРОМ MODE2
// ============================================================================
void mode2_timer_start();                    // Запустить таймер
void mode2_timer_stop();                     // Остановить таймер
void mode2_timer_reset();                     // Сбросить таймер
void mode2_timer_update();                    // Обновить состояние (вызывать в цикле)
String mode2_timer_get_formatted();           // Получить отформатированное время (ЧЧ:ММ)
bool mode2_timer_is_active();                 // Проверить, активен ли таймер
uint32_t mode2_timer_get_seconds();           // Получить количество секунд (для логики)

#endif