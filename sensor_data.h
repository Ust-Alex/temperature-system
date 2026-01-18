// sensor_data.h
// Структура данных для передачи между задачами через очередь RTOS

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h> // Для uint8_t и подобных типов

// Структура пакета данных, который будет передаваться в очереди
typedef struct {
    float temp_main;       // Основная температура ("гильза") для логики системы
    float temp_info1;      // Доп. температура 1 (только для информации)
    float temp_info2;      // Доп. температура 2
    float temp_info3;      // Доп. температура 3
    uint8_t system_mode;   // Текущий режим работы системы (для фона на дисплее/телефоне)
    bool alarm_status;     // Флаг тревоги (true/false). Связан с оповещением.
} sensor_data_t;

// Объявление ГЛОБАЛЬНОЙ очереди как внешней (extern)
// Сама очередь создается в temperature_system.ino
extern QueueHandle_t xSensorDataQueue;

#endif // SENSOR_DATA_H