/**
 * ============================================================================
 * ФАЙЛ: measurement_core.cpp
 * ЯДРО ИЗМЕРЕНИЙ - ФУНКЦИИ ОБРАБОТКИ ДАННЫХ
 * 
 * ВЕРСИЯ: 6.0 (ДЕЛЬТА ПОЛНОСТЬЮ УДАЛЕНА)
 * 
 * ОТВЕТСТВЕННОСТЬ:
 * 1. Фильтрация данных с датчиков
 * 2. Безопасная передача данных между задачами
 * 3. Звуковое оповещение о критических событиях
 * ============================================================================
 */

#include "measurement_core.h"
#include "mp3_player.h"

// ============================================================================
// ФИЛЬТРАЦИЯ ДАННЫХ
// ============================================================================

float filterValue(int sensorIdx, float newValue) {
  if (sensorIdx < 0 || sensorIdx >= 4) return newValue;

  Sensor_t* s = &sensors[sensorIdx];
  static bool bufferInitialized[4] = { false };

  if (!bufferInitialized[sensorIdx]) {
    for (int j = 0; j < 5; j++) {
      s->filterBuffer[j] = newValue;
    }
    s->filterSum = newValue * 5.0f;
    s->filterIndex = 0;
    bufferInitialized[sensorIdx] = true;
    return newValue;
  }

  s->filterSum -= s->filterBuffer[s->filterIndex];
  s->filterBuffer[s->filterIndex] = newValue;
  s->filterSum += newValue;
  s->filterIndex = (s->filterIndex + 1) % 5;

  return s->filterSum / 5.0f;
}

bool isValidTemperature(float temp) {
  return !(temp == TEMP_NO_DATA || temp == TEMP_SENSOR_LOST || temp == TEMP_CRITICAL_LOST || temp < -50.0f || temp > 150.0f);
}

// ============================================================================
// БЕЗОПАСНЫЙ ДОСТУП К ДАННЫМ (БЕЗ ДЕЛЬТЫ)
// ============================================================================

void safeUpdateSystemData(int idx, float temp, float delta) {
  // Параметр delta сохранён для совместимости с вызовами, но не используется
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    sysData.temps[idx] = temp;
    // sysData.deltas[idx] = delta;  // УДАЛЕНО - поля deltas больше нет
    xSemaphoreGive(dataMutex);
  }
}

void safeReadSystemData(SystemData_t* data) {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    memcpy(data, &sysData, sizeof(SystemData_t));
    xSemaphoreGive(dataMutex);
  }
}

// ============================================================================
// ЗВУКОВЫЕ ОПОВЕЩЕНИЯ
// ============================================================================

static void playCriticalAlarm() {
  if (!mp3PlayerReady) return;

  Mp3Command_t cmdStop = { MP3_CMD_STOP, 0 };
  sendMP3Command(cmdStop);
  vTaskDelay(pdMS_TO_TICKS(100));

  Mp3Command_t cmdAlert = { MP3_CMD_PLAY_TRACK, 5 };  // 0005Avaria.mp3
  sendMP3Command(cmdAlert);
}

static void stopAllSounds() {
  if (!mp3PlayerReady) return;

  Mp3Command_t cmdStop = { MP3_CMD_STOP, 0 };
  sendMP3Command(cmdStop);

  Mp3Command_t cmdDisableRepeat = { MP3_CMD_DISABLE_REPEAT, 0 };
  sendMP3Command(cmdDisableRepeat);
}

// ============================================================================
// ОБРАБОТКА КРИТИЧЕСКИХ СОБЫТИЙ
// ============================================================================

void handleCriticalError() {
  Serial.println("[ERROR] 🚨 Критическая ошибка (потеря гильзы)!");

  playCriticalAlarm();

  timeIsCounting = false;
  timeStartMs = 0;

  if (baseSaved) {
    baseSaved = false;
    guildBaseTemp = 0.0f;
  }
}

void handleCriticalErrorRecovery() {
  Serial.println("[INFO] ✅ Критическая ошибка снята");
  stopAllSounds();  // Останавливаем все звуки при восстановлении
}