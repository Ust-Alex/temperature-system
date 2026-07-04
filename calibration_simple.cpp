/**
 * @file calibration_simple.cpp
 * @brief Калибровка через offset. Все данные в EEPROM, локальных дублей нет.
 * @version 4.1 (КАЛИБРОВКА ТОЛЬКО СТЕНОК ПО ГИЛЬЗЕ)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - Гильза (индекс 4) — жёсткий эталон
 * - Калибруются только стенки (индексы 1,2,3)
 * - Выход (0) и Куб (5) не калибруются
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
  // ========================================================================
  // Проверяем наличие гильзы (эталон)
  // ========================================================================
  if (!sensors[4].found) {
    Serial.println("[CALIB] Невозможно выполнить калибровку: датчик гильзы (индекс 4) отсутствует");
    return;
  }

  float refTemp = sensors[4].temp;
  Serial.printf("[CALIB] Эталон: ГИЛЬЗА [4] %.2f°C\n", refTemp);

  // ========================================================================
  // Калибруем только стенки (индексы 1,2,3)
  // ========================================================================
  for (int i = 1; i <= 3; i++) {
    if (sensors[i].found) {
      float newOffset = -(sensors[i].temp - refTemp);
      settings_set_offset(i, newOffset);
      Serial.printf("  [%d] %s: offset %+.2f°C\n", i, sensorNames[i], newOffset);
    } else {
      Serial.printf("  [%d] %s: ДАТЧИК НЕ НАЙДЕН, пропускаем\n", i, sensorNames[i]);
    }
  }

  // ========================================================================
  // Сбрасываем offset для гильзы (эталон) и других датчиков (0,5)
  // ========================================================================
  settings_set_offset(4, 0.0f);  // Гильза
  settings_set_offset(0, 0.0f);  // Выход (не калибруется)
  settings_set_offset(5, 0.0f);  // Куб (не калибруется)

  settings_save();
  Serial.println("[CALIB] Готово");
}

// ============================================================================
void setManualOffset(int idx, float offset) {
  // Разрешаем ручную калибровку только для стенок (1,2,3)
  if (idx < 1 || idx > 3) {
    Serial.println("[CALIB] Ручная калибровка разрешена только для стенок (индексы 1,2,3)");
    return;
  }
  settings_set_offset(idx, offset);
  settings_save();
  Serial.printf("[CALIB] [%d] %s: offset = %+.2f°C\n", idx, sensorNames[idx], offset);
}

// ============================================================================
void setReferenceSensor(int idx) {
  // Запрещаем менять эталон — гильза всегда эталон
  Serial.println("[CALIB] Эталон всегда ГИЛЬЗА (индекс 4). Изменение запрещено.");
}

// ============================================================================
void toggleCalibration(bool enable) {
  settings_set_calibration_enabled(enable);
  settings_save();
  Serial.printf("[CALIB] Калибровка %s\n", enable ? "ВКЛ" : "ВЫКЛ");
}

// ============================================================================
float applyCalibration(int idx, float temp) {
  if (idx < 0 || idx >= 6) return temp;
  if (!settings_get_calibration_enabled()) return temp;
  // Калибровка применяется только к стенкам (1,2,3)
  if (idx >= 1 && idx <= 3) {
    return temp + settings_get_offset(idx);
  }
  return temp;  // Для гильзы, выхода и куба — без калибровки
}

// ============================================================================
void printCalibrationStatus() {
  Serial.println("\n" + String(40, '='));
  Serial.println("СТАТУС КАЛИБРОВКИ");
  Serial.println(String(40, '='));

  Serial.println("Эталон: ГИЛЬЗА [4] (фиксированный)");
  Serial.printf("Активна: %s\n", settings_get_calibration_enabled() ? "ДА" : "НЕТ");

  Serial.println("\nКоэфф. offset (только для стенок):");
  for (int i = 1; i <= 3; i++) {
    if (sensors[i].found) {
      float raw = sensors[i].temp;
      float cal = applyCalibration(i, raw);
      Serial.printf("  [%d] %s: %.2f -> %.2f (offset %+.2f)\n",
                    i, sensorNames[i], raw, cal, settings_get_offset(i));
    } else {
      Serial.printf("  [%d] %s: ДАТЧИК НЕ НАЙДЕН\n", i, sensorNames[i]);
    }
  }

  Serial.println("\nДругие датчики (без калибровки):");
  int other[] = {0, 4, 5};
  for (int j = 0; j < 3; j++) {
    int i = other[j];
    if (sensors[i].found) {
      Serial.printf("  [%d] %s: %.2f°C\n", i, sensorNames[i], sensors[i].temp);
    } else {
      Serial.printf("  [%d] %s: ДАТЧИК НЕ НАЙДЕН\n", i, sensorNames[i]);
    }
  }
  Serial.println(String(40, '='));
}