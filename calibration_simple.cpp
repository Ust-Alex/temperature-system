/**
 * ============================================================================
 * ФАЙЛ: calibration_simple.cpp
 * ПРОСТАЯ КАЛИБРОВКА - ПОПРАВОЧНЫЕ КОЭФФИЦИЕНТЫ
 * 
 * ЦЕЛЬ: Выровнять показания всех датчиков между собой
 * МЕТОД: offset-поправка относительно выбранного датчика-эталона
 * ============================================================================
 */

#include "calibration_simple.h"
#include <EEPROM.h>

// ============================================================================
// ПРОСТЫЕ КОЭФФИЦИЕНТЫ (ТОЛЬКО OFFSET)
// ============================================================================

float calibrationOffsets[4] = {0, 0, 0, 0};  // Поправки для датчиков 0-3
int referenceSensor = 3;                     // Датчик-эталон (50см по умолчанию)
bool calibrationEnabled = true;              // Включить калибровку

// ============================================================================
// РАБОТА С EEPROM (ПРОСТАЯ)
// ============================================================================

void saveOffsetsToEEPROM() {
    EEPROM.begin(64);  // Минимальный размер
    
    // Сохраняем флаг калибровки (0xAA = включена)
    EEPROM.write(0, 0xAA);
    
    // Сохраняем индексы: 1 = referenceSensor, 2-5 = calibrationEnabled
    EEPROM.write(1, referenceSensor);
    EEPROM.write(2, calibrationEnabled ? 1 : 0);
    
    // Сохраняем 4 значения float (16 байт)
    int addr = 4;
    for (int i = 0; i < 4; i++) {
        EEPROM.put(addr, calibrationOffsets[i]);
        addr += sizeof(float);
    }
    
    EEPROM.commit();
    Serial.println("[CALIB] Коэффициенты сохранены в EEPROM");
}

void loadOffsetsFromEEPROM() {
    EEPROM.begin(64);
    
    // Проверяем флаг калибровки
    if (EEPROM.read(0) != 0xAA) {
        Serial.println("[CALIB] EEPROM пуста, калибровка не загружена");
        return;
    }
    
    // Загружаем настройки
    referenceSensor = EEPROM.read(1);
    calibrationEnabled = (EEPROM.read(2) == 1);
    
    // Загружаем коэффициенты
    int addr = 4;
    for (int i = 0; i < 4; i++) {
        EEPROM.get(addr, calibrationOffsets[i]);
        addr += sizeof(float);
    }
    
    Serial.println("[CALIB] Коэффициенты загружены из EEPROM:");
    for (int i = 0; i < 4; i++) {
        Serial.printf("  [%d] %s: %+.2f°C\n", i, sensorNames[i], calibrationOffsets[i]);
    }
}

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ КАЛИБРОВКИ
// ============================================================================

/**
 * АВТОМАТИЧЕСКАЯ КАЛИБРОВКА ОТНОСИТЕЛЬНО ЭТАЛОНА
 * Все датчики должны быть в одинаковых температурных условиях!
 */
void autoCalibrateAllSensors() {
    if (!sensors[referenceSensor].found) {
        Serial.println("[CALIB] Ошибка: датчик-эталон не найден!");
        return;
    }
    
    float referenceTemp = sensors[referenceSensor].temp;
    Serial.printf("[CALIB] Эталон (%s): %.2f°C\n", 
                  sensorNames[referenceSensor], referenceTemp);
    
    for (int i = 0; i < 4; i++) {
        if (sensors[i].found && i != referenceSensor) {
            float diff = sensors[i].temp - referenceTemp;
            calibrationOffsets[i] = -diff;  // Отрицательная разница = поправка
            
            Serial.printf("  [%d] %s: %.2f°C, разница: %+.2f°C, offset: %+.2f°C\n",
                         i, sensorNames[i], sensors[i].temp, diff, calibrationOffsets[i]);
        }
    }
    
    // Эталонный датчик всегда с offset = 0
    calibrationOffsets[referenceSensor] = 0.0f;
    
    saveOffsetsToEEPROM();
    Serial.println("[CALIB] Автокалибровка завершена!");
}

/**
 * РУЧНАЯ УСТАНОВКА ПОПРАВКИ ДЛЯ ДАТЧИКА
 */
void setManualOffset(int sensorIdx, float offset) {
    if (sensorIdx < 0 || sensorIdx >= 4) return;
    
    calibrationOffsets[sensorIdx] = offset;
    Serial.printf("[CALIB] Датчик [%d] %s: offset = %+.2f°C\n",
                 sensorIdx, sensorNames[sensorIdx], offset);
    
    saveOffsetsToEEPROM();
}

/**
 * СМЕНА ДАТЧИКА-ЭТАЛОНА
 */
void setReferenceSensor(int sensorIdx) {
    if (!sensors[sensorIdx].found) {
        Serial.println("[CALIB] Ошибка: датчик не найден!");
        return;
    }
    
    referenceSensor = sensorIdx;
    calibrationOffsets[sensorIdx] = 0.0f;  // Эталон всегда 0
    
    Serial.printf("[CALIB] Новый эталон: [%d] %s\n", 
                 sensorIdx, sensorNames[sensorIdx]);
    saveOffsetsToEEPROM();
}

// ============================================================================
// ПРИМЕНЕНИЕ ПОПРАВОК
// ============================================================================

/**
 * ПРИМЕНИТЬ ПОПРАВКУ К ТЕМПЕРАТУРЕ
 * Использовать ВМЕСТО обычной filterValue!
 */
float applyCalibration(int sensorIdx, float temperature) {
    if (!calibrationEnabled || sensorIdx < 0 || sensorIdx >= 4) {
        return temperature;
    }
    
    return temperature + calibrationOffsets[sensorIdx];
}

/**
 * ФУНКЦИЯ ФИЛЬТРАЦИИ С КАЛИБРОВКОЙ
 * ЗАМЕНИТЕ ВАШУ filterValue на эту функцию!
 */
float filterValueWithCalibration(int sensorIdx, float newValue) {
    // 1. Фильтрация (ваш существующий код)
    float filtered = filterValue(sensorIdx, newValue);
    
    // 2. Применение калибровки
    return applyCalibration(sensorIdx, filtered);
}

// ============================================================================
// СЕРВИСНЫЕ ФУНКЦИИ
// ============================================================================

void printCalibrationStatus() {
    Serial.println("\n" + String(50, '='));
    Serial.println("СТАТУС КАЛИБРОВКИ");
    Serial.println(String(50, '='));
    
    Serial.printf("Эталон: [%d] %s\n", referenceSensor, sensorNames[referenceSensor]);
    Serial.printf("Калибровка: %s\n", calibrationEnabled ? "ВКЛ" : "ВЫКЛ");
    
    Serial.println("\nПоправочные коэффициенты:");
    for (int i = 0; i < 4; i++) {
        if (sensors[i].found) {
            float raw = sensors[i].temp;
            float calibrated = applyCalibration(i, raw);
            
            Serial.printf("  [%d] %s: %.2f°C -> %.2f°C (offset: %+.2f°C)\n",
                         i, sensorNames[i], raw, calibrated, calibrationOffsets[i]);
        }
    }
    
    Serial.println(String(50, '='));
}

void toggleCalibration(bool enable) {
    calibrationEnabled = enable;
    Serial.printf("[CALIB] Калибровка %s\n", enable ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
    saveOffsetsToEEPROM();
}