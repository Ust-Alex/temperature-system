#include "rtos_tasks.h"

void taskMeasure(void* pv) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  uint32_t lastDeltaTime = 0;
  float lastTemps[4] = { 0, 0, 0, 0 };
  uint32_t lastReconnectAttempt = 0;

  Serial.println("📡 Задача измерений запущена");

  while (1) {
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());

    if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
      attemptReconnect();
      lastReconnectAttempt = currentMillis;
    }

    if (criticalError || !sensors[3].found) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (sensors[3].found) {
      sensorsA.requestTemperatures();
    }

    bool busBNeeded = false;
    for (int i = 0; i < 3; i++) {
      if (sensors[i].found) {
        busBNeeded = true;
        break;
      }
    }

    if (busBNeeded) {
      sensorsB.requestTemperatures();
    }

    vTaskDelay(pdMS_TO_TICKS(750));

    for (int i = 0; i < 4; i++) {
      if (sensors[i].found) {
        float rawTemp = 0;

        if (i == 3) {
          rawTemp = sensorsA.getTempC(sensors[i].addr);
        } else {
          rawTemp = sensorsB.getTempC(sensors[i].addr);
        }

        if (rawTemp == DEVICE_DISCONNECTED_C || !isValidTemperature(rawTemp)) {
          sensors[i].temp = TEMP_NO_DATA;
          safeUpdateSystemData(i, TEMP_NO_DATA, 0.0f);
          sensors[i].lostTimer++;

          if (sensors[i].lostTimer > 10) {
            sensors[i].found = false;
            if (i == 3) {
              criticalError = true;
              systemInitialized = false;
            }
            Serial.printf("⚠️  %s помечен как потерянный\n", sensorNames[i]);
          }
        } else {
          sensors[i].lostTimer = 0;
          sensors[i].temp = filterValue(i, rawTemp);

          if (currentMillis - lastDeltaTime > DELTA_CALC_INTERVAL) {
            if (lastTemps[i] != 0) {
              float delta = sensors[i].temp - lastTemps[i];
              safeUpdateSystemData(i, sensors[i].temp, delta);
            } else {
              safeUpdateSystemData(i, sensors[i].temp, 0.0f);
            }
            lastTemps[i] = sensors[i].temp;
          } else {
            safeUpdateSystemData(i, sensors[i].temp, sysData.deltas[i]);
          }
        }
      } else {
        if (i == 3) {
          safeUpdateSystemData(i, TEMP_CRITICAL_LOST, 0.0f);
          criticalError = true;
          systemInitialized = false;
        } else {
          safeUpdateSystemData(i, TEMP_SENSOR_LOST, 0.0f);
        }
      }
    }

    if (currentMillis - lastDeltaTime > DELTA_CALC_INTERVAL) {
      lastDeltaTime = currentMillis;
    }

    if (dataQueue != NULL) {
      SystemData_t dataToSend;
      safeReadSystemData(&dataToSend);

      if (xQueueSend(dataQueue, &dataToSend, 0) != pdTRUE) {
        static uint32_t lastQueueError = 0;
        if (currentMillis - lastQueueError > 5000) {
          Serial.println("⚠️  Очередь данных переполнена (дисплей не успевает?)");
          lastQueueError = currentMillis;
        }
      }
    }

    if (sysData.mode == 0 && sensors[3].found) {
      float guildTemp = sysData.temps[3];
      mode1_update_stabilization_timer(guildTemp);
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(MEASURE_INTERVAL));
  }
}

void taskDisplay(void* pv) {
  SystemData_t displayData;
  SystemData_t localData;
  uint32_t lastUpdate = 0;

  Serial.println("🖥️  Задача дисплея запущена");

  while (1) {
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());
    criticalError = !sensors[3].found;

    if (dataQueue != NULL) {
      if (xQueueReceive(dataQueue, &displayData, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&localData, &displayData, sizeof(SystemData_t));

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          sysData.mode = localData.mode;
          sysData.needsRedraw = localData.needsRedraw;

          if (!forceDisplayRedraw) {
            memcpy(sysData.temps, localData.temps, sizeof(float) * 4);
            memcpy(sysData.deltas, localData.deltas, sizeof(float) * 4);
          }
          xSemaphoreGive(dataMutex);
        }

        if (sysData.mode != lastDisplayMode) {
          Serial.printf("[DISPLAY] Обнаружена смена режима: %d -> %d\n",
                        lastDisplayMode, sysData.mode);
          lastDisplayMode = sysData.mode;
          forceDisplayRedraw = true;
        }

        if (currentMillis - lastUpdate >= DISPLAY_UPDATE_MS) {
          if (sysData.mode == 1 && sensors[3].found && guildBaseTemp != 0.0f) {
            float currentGuildTemp = sysData.temps[3];
            mode2_update_color_state(currentGuildTemp);
          }

          if (sysData.mode == 0) {
            updateDisplayMODE1();
          } else {
            switch (guildColorState) {
              case 0:
                updateDisplayMODE2_GREEN();
                break;
              case 1:
                updateDisplayMODE2_YELLOW();
                break;
              case 2:
                updateDisplayMODE2_RED();
                break;
              default:
                updateDisplayMODE1();
                break;
            }
          }

          lastUpdate = currentMillis;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void taskSerial(void* pv) {
  Serial.println("📟 Задача Serial запущена");

  while (1) {
    serial_handle_input();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void create_rtos_tasks() {
  Serial.println("\n" + String(50, '='));
  Serial.println("СОЗДАНИЕ ЗАДАЧ FREERTOS");
  Serial.println(String(50, '='));

  if (xTaskCreatePinnedToCore(
        taskMeasure,
        "MeasureTask",
        8192,
        NULL,
        3,
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ ОШИБКА: Не удалось создать задачу измерений!");
  } else {
    Serial.println("✅ Задача измерений создана (приоритет 3, ядро 1)");
  }

  if (xTaskCreatePinnedToCore(
        taskDisplay,
        "DisplayTask",
        12288,
        NULL,
        2,
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ ОШИБКА: Не удалось создать задачу дисплея!");
  } else {
    Serial.println("✅ Задача дисплея создана (приоритет 2, ядро 1)");
  }

  if (xTaskCreatePinnedToCore(
        taskSerial,
        "SerialTask",
        4096,
        NULL,
        1,
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ ОШИБКА: Не удалось создать задачу Serial!");
  } else {
    Serial.println("✅ Задача Serial создана (приоритет 1, ядро 1)");
  }

  Serial.println("\n" + String(60, '='));
  Serial.println("✅ СИСТЕМА УСПЕШНО ЗАПУЩЕНА");
  Serial.println(String(60, '='));
  Serial.println("Задачи FreeRTOS:");
  Serial.println("  📡 MeasureTask - измерения (приоритет 3, стек 8КБ)");
  Serial.println("  🖥️  DisplayTask - дисплей (приоритет 2, стек 12КБ)");
  Serial.println("  📟 SerialTask  - команды (приоритет 1, стек 4КБ)");
  Serial.println("\n🔥 Система готова к работе!");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println(String(60, '=') + "\n");
}