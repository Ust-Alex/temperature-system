/**
 * ============================================================================
 * ФАЙЛ: measurement_task.cpp
 * ЗАДАЧА ИЗМЕРЕНИЙ ТЕМПЕРАТУРЫ (6 ДАТЧИКОВ)
 * ВЕРСИЯ: 5.0
 * ============================================================================
 */

#include "measurement_task.h"
#include "globals.h"
#include "system_config.h"
#include "sensors.h"
#include "mode1_logic.h"
#include "mode2_logic.h"
#include "mode2_sounds.h"

static uint32_t lastHeartbeat = 0;
static uint32_t measurementCount = 0;

// ============================================================================
// ОСНОВНАЯ ЗАДАЧА ИЗМЕРЕНИЙ
// ============================================================================
void taskMeasure(void* pvParameters) {
  Serial.println("📡 Задача измерений запущена (6 датчиков)");

  while (1) {
    TickType_t cycleStartTime = xTaskGetTickCount();
    uint32_t currentMillis = millis();
    measurementCount++;

    // Heartbeat
    if (currentMillis - lastHeartbeat > 30000) {
      lastHeartbeat = currentMillis;
    }

    // ОПРОС ДАТЧИКОВ
    sensors_update_all();

    // ОБНОВЛЕНИЕ sysData (6 датчиков)
    for (int i = 0; i < 6; i++) {
      if (sensors[i].found && sensors[i].temp != TEMP_NO_DATA) {
        safeUpdateSystemData(i, sensors[i].temp, 0.0f);
      } else {
        float errTemp = (i == 4) ? TEMP_NO_DATA : TEMP_SENSOR_LOST;
        safeUpdateSystemData(i, errTemp, 0.0f);
      }
    }

    // ОТПРАВКА ДАННЫХ В ОЧЕРЕДЬ ДЛЯ ДИСПЛЕЯ
    if (dataQueue != NULL) {
      SystemData_t dataToSend;
      safeReadSystemData(&dataToSend);
      xQueueSend(dataQueue, &dataToSend, 0);
    }

    // ОБНОВЛЕНИЕ ТАЙМЕРА СТАБИЛИЗАЦИИ (гильза индекс 4)
    if (sysData.mode == 0 && sensors[4].found) {
      mode1_update_stabilization_timer(sensors[4].temp);
    }

    // ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ (гильза индекс 4)
    if (sysData.mode == 1 && sensors[4].found) {
      mode2_update_color_state(sensors[4].temp);
    }

    // ОБНОВЛЕНИЕ ЗВУКОВ
    if (sysData.mode == 1 && sensors[4].found) {
      mode2_sounds_update();
    }

    // ТОЧНЫЙ ЦИКЛ
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - cycleStartTime;
    TickType_t targetCycle = pdMS_TO_TICKS(MEASURE_INTERVAL);

    if (elapsed < targetCycle) {
      vTaskDelay(targetCycle - elapsed);
    }
  }
}