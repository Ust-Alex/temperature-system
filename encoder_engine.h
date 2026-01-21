#ifndef ENCODER_ENGINE_H
#define ENCODER_ENGINE_H

#include <Arduino.h>

// ============================================================================
// ТИПЫ СОБЫТИЙ ЭНКОДЕРА
// ============================================================================
typedef enum {
    EVENT_NONE,             // Нет события
    EVENT_BUTTON_CLICK,     // Короткое нажатие кнопки
    EVENT_BUTTON_DOUBLE,    // Двойное нажатие кнопки
    EVENT_ENCODER_LEFT,     // Поворот энкодера влево
    EVENT_ENCODER_RIGHT     // Поворот энкодера вправо
} EncoderEvent_t;

// ============================================================================
// ОБЪЯВЛЕНИЕ ФУНКЦИЙ
// ============================================================================
void encoder_init();                    // Инициализация энкодера
EncoderEvent_t encoder_tick();          // Опрос энкодера, возвращает событие
void encoder_reset_inactivity_timer();  // Сброс таймера неактивности
uint32_t encoder_get_inactivity_time(); // Получение времени неактивности

#endif // ENCODER_ENGINE_H