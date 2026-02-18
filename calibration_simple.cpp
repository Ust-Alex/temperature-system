/**
 * ============================================================================
 * ФАЙЛ: calibration_simple.cpp
 * ПРОСТАЯ КАЛИБРОВКА - ПОПРАВОЧНЫЕ КОЭФФИЦИЕНТЫ
 *
 * ЦЕЛЬ: Выровнять показания всех датчиков между собой
 * МЕТОД: offset-поправка относительно выбранного датчика-эталона
 *
 * ВЕРСИЯ: 2.0 (ИНТЕГРАЦИЯ С EEPROM_SETTINGS)
 * ============================================================================
 */

#include "calibration_simple.h"
#include "eeprom_settings.h" // Для доступа к настройкам
#include "sensors.h"         // Для доступа к sensors[].found и sensors[].temp
#include "system_config.h"   // Для sensorNames

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (теперь они берутся из eeprom_settings)
// ============================================================================
// Эти переменные оставлены для обратной совместимости,
// но их значения теперь управляются через модуль настроек.
float calibrationOffsets[4] = {0, 0, 0, 0};
int referenceSensor = 3;
bool calibrationEnabled = true;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МОДУЛЯ (загружает текущие значения из настроек)
// ============================================================================
void calibration_init() {
    // Загружаем текущие значения из глобальных настроек в локальные переменные
    for (int i = 0; i < 4; i++) {
        calibrationOffsets[i] = settings_get_offset(i);
    }
    referenceSensor = settings_get_reference();
    calibrationEnabled = settings_get()->calibrationEnabled; // Нужна будет функция-геттер

    Serial.println("[CALIB] Модуль инициализирован, данные загружены из EEPROM.");
    printCalibrationStatus();
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
            float newOffset = -diff;
            calibrationOffsets[i] = newOffset;
            settings_set_offset(i, newOffset); // Сохраняем через модуль настроек

            Serial.printf("  [%d] %s: %.2f°C, разница: %+.2f°C, offset: %+.2f°C\n",
                         i, sensorNames[i], sensors[i].temp, diff, newOffset);
        }
    }

    // Эталонный датчик всегда с offset = 0
    calibrationOffsets[referenceSensor] = 0.0f;
    settings_set_offset(referenceSensor, 0.0f);

    Serial.println("[CALIB] Автокалибровка завершена!");
    settings_save(); // Сохраняем все настройки разом
}

/**
 * РУЧНАЯ УСТАНОВКА ПОПРАВКИ ДЛЯ ДАТЧИКА
 */
void setManualOffset(int sensorIdx, float offset) {
    if (sensorIdx < 0 || sensorIdx >= 4) return;

    calibrationOffsets[sensorIdx] = offset;
    settings_set_offset(sensorIdx, offset);
    settings_save();

    Serial.printf("[CALIB] Датчик [%d] %s: offset = %+.2f°C\n",
                 sensorIdx, sensorNames[sensorIdx], offset);
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
    calibrationOffsets[sensorIdx] = 0.0f; // Эталон всегда 0

    settings_set_reference(sensorIdx);
    settings_set_offset(sensorIdx, 0.0f);
    settings_save();

    Serial.printf("[CALIB] Новый эталон: [%d] %s\n",
                 sensorIdx, sensorNames[sensorIdx]);
}

// ============================================================================
// ПРИМЕНЕНИЕ ПОПРАВОК
// ============================================================================

/**
 * ПРИМЕНИТЬ ПОПРАВКУ К ТЕМПЕРАТУРЕ
 */
float applyCalibration(int sensorIdx, float temperature) {
    // Обновляем флаг из настроек на всякий случай
    calibrationEnabled = settings_get()->calibrationEnabled;

    if (!calibrationEnabled || sensorIdx < 0 || sensorIdx >= 4) {
        return temperature;
    }
    return temperature + calibrationOffsets[sensorIdx];
}

/**
 * ФУНКЦИЯ ФИЛЬТРАЦИИ С КАЛИБРОВКОЙ
 */
float filterValueWithCalibration(int sensorIdx, float newValue) {
    // 1. Фильтрация
    float filtered = filterValue(sensorIdx, newValue);
    // 2. Применение калибровки
    return applyCalibration(sensorIdx, filtered);
}

// ============================================================================
// СЕРВИСНЫЕ ФУНКЦИИ
// ============================================================================

void printCalibrationStatus() {
    // Обновляем локальные копии перед печатью
    for (int i = 0; i < 4; i++) {
        calibrationOffsets[i] = settings_get_offset(i);
    }
    referenceSensor = settings_get_reference();
    calibrationEnabled = settings_get()->calibrationEnabled;

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
    // Здесь нужно будет получить доступ к полю структуры settings
    // и изменить его. Для этого лучше добавить функцию в eeprom_settings:
    // settings_set_calibration_enabled(enable);
    Serial.printf("[CALIB] Калибровка %s\n", enable ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
    // settings_save();
}