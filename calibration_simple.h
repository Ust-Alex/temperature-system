#ifndef CALIBRATION_SIMPLE_H
#define CALIBRATION_SIMPLE_H

#include "system_config.h"

// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (extern - объявление)
extern float calibrationOffsets[4];
extern int referenceSensor;
extern bool calibrationEnabled;

// Загрузка/сохранение
void loadOffsetsFromEEPROM();
void saveOffsetsToEEPROM();

// Калибровка
void autoCalibrateAllSensors();
void setManualOffset(int sensorIdx, float offset);
void setReferenceSensor(int sensorIdx);
void toggleCalibration(bool enable);

// Применение
float applyCalibration(int sensorIdx, float temperature);
float filterValueWithCalibration(int sensorIdx, float newValue);

// Отображение
void printCalibrationStatus();

#endif