/**
 * ============================================================================
 * ФАЙЛ: measurement_task.cpp
 * ЗАДАЧА ИЗМЕРЕНИЙ ТЕМПЕРАТУРЫ (ВЫДЕЛЕНА В ОТДЕЛЬНЫЙ МОДУЛЬ)
 * 
 * ВЕРСИЯ: 3.0 (ИЗМЕНЕНА: РАБОТАЕТ БЕЗ ДАТЧИКА ГИЛЬЗЫ)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - УБРАНА блокирующая проверка if (!systemInitialized)
 * - При отсутствии гильзы в sysData.temps[3] записывается TEMP_NO_DATA
 * - Для остальных датчиков логика не изменилась
 * ============================================================================
 */

#include "measurement_task.h"
#include "globals.h"
#include "system_config.h"
#include "sensors.h"
#include "mode1_logic.h"
#include "mode2_logic.h"
#include "mode2_sounds.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// ============================================================================
static uint32_t lastDeltaTime = 0;          // Время последнего расчёта дельты
static float lastTemps[4] = { 0, 0, 0, 0 }; // Предыдущие температуры для расчёта дельты
static uint32_t lastHeartbeat = 0;          // Время последнего heartbeat-сообщения
static uint32_t measurementCount = 0;       // Счётчик измерений (для отладки)

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: РАСЧЁТ ДЕЛЬТ
// ============================================================================
void measurement_update_delta() {
  uint32_t currentMillis = millis();
  
  // Проверяем, пришло ли время расчёта дельты
  if (currentMillis - lastDeltaTime > DELTA_CALC_INTERVAL) {
    for (int i = 0; i < 4; i++) {
      if (sensors[i].found && sensors[i].temp != TEMP_NO_DATA) {
        if (lastTemps[i] != 0 && lastTemps[i] != TEMP_NO_DATA) {
          float delta = sensors[i].temp - lastTemps[i];
          safeUpdateSystemData(i, sensors[i].temp, delta);
        } else {
          safeUpdateSystemData(i, sensors[i].temp, 0.0f);
        }
        lastTemps[i] = sensors[i].temp;
      } else {
        // ================================================================
        // ИЗМЕНЕНИЕ: для гильзы (i==3) используем TEMP_NO_DATA вместо TEMP_CRITICAL_LOST
        // Это гарантирует, что на дисплее покажется "--.--"
        // ================================================================
        float errTemp;
        if (i == 3) {
          errTemp = TEMP_NO_DATA;           // "--.--" на дисплее
        } else {
          errTemp = TEMP_SENSOR_LOST;        // пустое место на дисплее (как было)
        }
        safeUpdateSystemData(i, errTemp, 0.0f);
      }
    }
    lastDeltaTime = currentMillis;
  } else {
    // Если дельту не считаем, просто обновляем температуры
    for (int i = 0; i < 4; i++) {
      if (sensors[i].found && sensors[i].temp != TEMP_NO_DATA) {
        safeUpdateSystemData(i, sensors[i].temp, sysData.deltas[i]);
      } else {
        float errTemp;
        if (i == 3) {
          errTemp = TEMP_NO_DATA;
        } else {
          errTemp = TEMP_SENSOR_LOST;
        }
        safeUpdateSystemData(i, errTemp, 0.0f);
      }
    }
  }
}

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: ОТПРАВКА ДАННЫХ В ДИСПЛЕЙ
// ============================================================================
void measurement_send_to_display() {
  if (dataQueue != NULL) {
    SystemData_t dataToSend;
    safeReadSystemData(&dataToSend);

    if (xQueueSend(dataQueue, &dataToSend, 0) != pdTRUE) {
      static uint32_t lastQueueError = 0;
      uint32_t now = millis();
      if (now - lastQueueError > 5000) {
        Serial.println("⚠️  [MEASURE] Очередь данных переполнена");
        lastQueueError = now;
      }
    }
  }
}

// ============================================================================
// ОСНОВНАЯ ЗАДАЧА ИЗМЕРЕНИЙ
// ============================================================================
void taskMeasure(void* pvParameters) {
  Serial.println("📡 Задача измерений запущена (отдельный модуль)");

  while (1) {
    TickType_t cycleStartTime = xTaskGetTickCount();
    uint32_t currentMillis = millis();
    measurementCount++;

    // ========================================================================
    // 1. HEARTBEAT ДЛЯ ОТЛАДКИ (раз в 30 секунд)
    // ========================================================================
    if (currentMillis - lastHeartbeat > 30000) {
      // Serial.printf("[MEASURE] Heartbeat: %lu ms, измерений: %lu\n",
      //               currentMillis, measurementCount);
      lastHeartbeat = currentMillis;
    }

    // ========================================================================
    // 2. ПРОВЕРКА ИНИЦИАЛИЗАЦИИ СИСТЕМЫ
    // ========================================================================
    // ИЗМЕНЕНИЕ: проверка УДАЛЕНА.
    // Раньше здесь было if (!systemInitialized) { vTaskDelay(...); continue; }
    // Теперь задача измерений работает ВСЕГДА, даже без гильзы.
    // ========================================================================

    // ========================================================================
    // 3. ОПРОС ДАТЧИКОВ ЧЕРЕЗ МОДУЛЬ SENSORS
    // ========================================================================
    sensors_update_all();

    // ========================================================================
    // 4. РАСЧЁТ ДЕЛЬТ И ОБНОВЛЕНИЕ sysData
    // ========================================================================
    measurement_update_delta();

    // ========================================================================
    // 5. ОТПРАВКА ДАННЫХ В ОЧЕРЕДЬ ДЛЯ ДИСПЛЕЯ
    // ========================================================================
    measurement_send_to_display();

    // ========================================================================
    // 6. ОБНОВЛЕНИЕ ТАЙМЕРА СТАБИЛИЗАЦИИ ДЛЯ MODE1
    // ========================================================================
    // ИЗМЕНЕНИЕ: добавлена проверка наличия гильзы
    // (сама функция mode1_update_stabilization_timer будет изменена отдельно)
    if (sysData.mode == 0 && sensors[3].found) {
      mode1_update_stabilization_timer(sensors[3].temp);
    }

    // ========================================================================
    // 7. ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ ДЛЯ MODE2
    // ========================================================================
    // ИЗМЕНЕНИЕ: добавлена проверка наличия гильзы
    // (сама функция mode2_update_color_state будет изменена отдельно)
    if (sysData.mode == 1 && sensors[3].found) {
      mode2_update_color_state(sensors[3].temp);
    }

    // ========================================================================
    // 8. ОБНОВЛЕНИЕ ЗВУКОВ В ЖЁЛТОМ РЕЖИМЕ
    // ========================================================================
    // ИЗМЕНЕНИЕ: добавлена проверка наличия гильзы
    if (sysData.mode == 1 && sensors[3].found) {
      mode2_sounds_update();
    }

    // ========================================================================
    // 9. ТОЧНЫЙ ЦИКЛ С ИНТЕРВАЛОМ MEASURE_INTERVAL
    // ========================================================================
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - cycleStartTime;
    TickType_t targetCycle = pdMS_TO_TICKS(MEASURE_INTERVAL);

    if (elapsed < targetCycle) {
      vTaskDelay(targetCycle - elapsed);
    }
  }
}