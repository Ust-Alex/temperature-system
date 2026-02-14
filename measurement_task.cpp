/** * ФАЙЛ: measurement_task.cpp
 * ЗАДАЧА ИЗМЕРЕНИЙ ТЕМПЕРАТУРЫ (ВЫДЕЛЕНА В ОТДЕЛЬНЫЙ МОДУЛЬ)
 * 
 * ВЕРСИЯ: 1.0 (С ИСПОЛЬЗОВАНИЕМ МОДУЛЯ SENSORS)
 * 
 * ОСОБЕННОСТИ:
 * 1. Работает как отдельная задача FreeRTOS
 * 2. Использует модуль sensors для опроса датчиков
 * 3. Рассчитывает дельты температур
 * 4. Отправляет данные в очередь для дисплея
 * 5. Обновляет таймер стабилизации MODE1
 * ============================================================================
 */

#include "measurement_task.h"
#include "globals.h"
#include "system_config.h"
#include "sensors.h"
#include "mode1_logic.h"
#include "mode2_logic.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// ============================================================================
static uint32_t lastDeltaTime = 0;           // Время последнего расчёта дельты
static float lastTemps[4] = { 0, 0, 0, 0 };  // Предыдущие температуры для расчёта дельты
static uint32_t lastHeartbeat = 0;           // Время последнего heartbeat-сообщения
static uint32_t measurementCount = 0;        // Счётчик измерений (для отладки)

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
        float errTemp = (i == 3) ? TEMP_CRITICAL_LOST : TEMP_SENSOR_LOST;
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
        float errTemp = (i == 3) ? TEMP_CRITICAL_LOST : TEMP_SENSOR_LOST;
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
    // Фиксируем начало цикла для точного интервала
    TickType_t cycleStartTime = xTaskGetTickCount();
    uint32_t currentMillis = millis();
    measurementCount++;

    // ========================================================================
    // 1. HEARTBEAT ДЛЯ ОТЛАДКИ (раз в 30 секунд)
    // ========================================================================
    if (currentMillis - lastHeartbeat > 30000) {  // 30 секунд
      // Serial.printf("[MEASURE] Heartbeat: %lu ms, измерений: %lu\n",
      //               currentMillis, measurementCount);
      lastHeartbeat = currentMillis;
    }

    // ========================================================================
    // 2. ПРОВЕРКА ИНИЦИАЛИЗАЦИИ СИСТЕМЫ
    // ========================================================================
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(MEASURE_INTERVAL));
      continue;
    }

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
    if (sysData.mode == 0 && sensors[3].found) {
      mode1_update_stabilization_timer(sensors[3].temp);
    }

    // ========================================================================
    // 7. ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ ДЛЯ MODE2
    // ========================================================================
    if (sysData.mode == 1 && sensors[3].found) {
      mode2_update_color_state(sensors[3].temp);
    }


    // ========================================================================
    // 8. ТОЧНЫЙ ЦИКЛ С ИНТЕРВАЛОМ MEASURE_INTERVAL
    // ========================================================================
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - cycleStartTime;
    TickType_t targetCycle = pdMS_TO_TICKS(MEASURE_INTERVAL);

    if (elapsed < targetCycle) {
      vTaskDelay(targetCycle - elapsed);
    }
  }
}