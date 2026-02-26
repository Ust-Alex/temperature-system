/**
 * @file calibration_simple.cpp
 * @brief Калибровка через offset. Все данные в EEPROM, локальных дублей нет.
 * @version 2.1
 */

#include "calibration_simple.h"
#include "eeprom_settings.h"
#include "sensors.h"
#include "system_config.h"

// ============================================================================
void calibration_init() {
    Serial.println("[CALIB] Модуль инициализирован");
    printCalibrationStatus();
}

// ============================================================================
void autoCalibrateAllSensors() {
    int ref = settings_get_reference();
    if (!sensors[ref].found) {
        Serial.println("[CALIB] Ошибка: эталон не найден");
        return;
    }

    float refTemp = sensors[ref].temp;
    Serial.printf("[CALIB] Эталон [%d] %s: %.2f°C\n", ref, sensorNames[ref], refTemp);

    for (int i = 0; i < 4; i++) {
        if (sensors[i].found && i != ref) {
            float newOffset = -(sensors[i].temp - refTemp);
            settings_set_offset(i, newOffset);
            Serial.printf("  [%d] %s: offset %+.2f°C\n", i, sensorNames[i], newOffset);
        }
    }
    settings_set_offset(ref, 0.0f);
    settings_save();
    Serial.println("[CALIB] Готово");
}

// ============================================================================
void setManualOffset(int idx, float offset) {
    if (idx < 0 || idx >= 4) return;
    settings_set_offset(idx, offset);
    settings_save();
    Serial.printf("[CALIB] [%d] %s: offset = %+.2f°C\n", idx, sensorNames[idx], offset);
}

// ============================================================================
void setReferenceSensor(int idx) {
    if (idx < 0 || idx >= 4 || !sensors[idx].found) {
        Serial.println("[CALIB] Ошибка: датчик не найден");
        return;
    }
    settings_set_reference(idx);
    settings_set_offset(idx, 0.0f);
    settings_save();
    Serial.printf("[CALIB] Новый эталон: [%d] %s\n", idx, sensorNames[idx]);
}

// ============================================================================
void toggleCalibration(bool enable) {
    settings_set_calibration_enabled(enable);
    settings_save();
    Serial.printf("[CALIB] Калибровка %s\n", enable ? "ВКЛ" : "ВЫКЛ");
}

// ============================================================================
float applyCalibration(int idx, float temp) {
    if (idx < 0 || idx >= 4) return temp;
    if (!settings_get_calibration_enabled()) return temp;
    return temp + settings_get_offset(idx);
}

// ============================================================================
void printCalibrationStatus() {
    Serial.println("\n" + String(40, '='));
    Serial.println("СТАТУС КАЛИБРОВКИ");
    Serial.println(String(40, '='));

    int ref = settings_get_reference();
    Serial.printf("Эталон: [%d] %s\n", ref, sensorNames[ref]);
    Serial.printf("Активна: %s\n", settings_get_calibration_enabled() ? "ДА" : "НЕТ");

    Serial.println("Коэфф. offset:");
    for (int i = 0; i < 4; i++) {
        if (sensors[i].found) {
            float raw = sensors[i].temp;
            float cal = applyCalibration(i, raw);
            Serial.printf("  [%d] %s: %.2f -> %.2f (offset %+.2f)\n",
                i, sensorNames[i], raw, cal, settings_get_offset(i));
        }
    }
    Serial.println(String(40, '='));
}