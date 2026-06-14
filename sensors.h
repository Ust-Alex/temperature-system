/**
 * ============================================================================
 * ФАЙЛ: sensors.h
 * ЗАГОЛОВОЧНЫЙ ФАЙЛ МОДУЛЯ УПРАВЛЕНИЯ ДАТЧИКАМИ DS18B20
 * ВЕРСИЯ: 3.0 (4 ШИНЫ, 6 ДАТЧИКОВ)
 * ============================================================================
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================================
// ГЛОБАЛЬНЫЕ ОБЪЕКТЫ (4 ШИНЫ)
// ============================================================================
extern OneWire oneWireA;
extern OneWire oneWireB;
extern OneWire oneWireC;
extern OneWire oneWireD;
extern DallasTemperature sensorsA;
extern DallasTemperature sensorsB;
extern DallasTemperature sensorsC;
extern DallasTemperature sensorsD;

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ
// ============================================================================
void sensors_init();                    // Инициализация всех шин и датчиков
void sensors_scan_all();               // Поиск датчиков на всех шинах
void sensors_request_temperatures();   // Запрос на измерение температуры
void sensors_read_temperatures();      // Чтение температур
void sensors_update_all();             // Полный цикл: запрос → чтение → фильтр → калибровка

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================
float sensors_get_temp(int idx);        // Получить температуру датчика по индексу
bool sensors_is_found(int idx);         // Проверить, найден ли датчик
float sensors_get_filtered(int idx);    // Получить отфильтрованную температуру

#endif // SENSORS_H