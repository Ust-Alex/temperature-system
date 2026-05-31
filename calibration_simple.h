#ifndef CALIBRATION_SIMPLE_H
#define CALIBRATION_SIMPLE_H

#include "system_config.h"

// ============================================================================
// КАЛИБРОВКА ДАТЧИКОВ (ИНТЕРФЕЙС)
// Все данные хранятся в eeprom_settings, глобальных переменных больше нет.
// ============================================================================

void calibration_init();

// Калибровка
void autoCalibrateAllSensors();               // Авто по эталону
void setManualOffset(int idx, float offset);  // Ручная установка
void setReferenceSensor(int idx);             // Смена эталона
void toggleCalibration(bool enable);          // Вкл/Выкл

// Применение
float applyCalibration(int idx, float temp);  // Собственно калибровка

// Статус
void printCalibrationStatus();

#endif