#include "rtos_tasks.h"

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ ДЛЯ ОТЛАДКИ
// ============================================================================
#define HEARTBEAT_INTERVAL 30000     // 30 секунд
#define STACK_CHECK_INTERVAL 300000  // 5 минут

// ============================================================================
// ЗАДАЧА ИЗМЕРЕНИЙ (С УЛУЧШЕНИЯМИ)
// ============================================================================
void taskMeasure(void* pv) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  uint32_t lastDeltaTime = 0;
  float lastTemps[4] = { 0, 0, 0, 0 };
  uint32_t lastReconnectAttempt = 0;
  uint32_t lastHeartbeat = 0;
  uint32_t measurementCount = 0;

  Serial.println("📡 Задача измерений запущена");

  while (1) {
    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());
    measurementCount++;

    // 1. Heartbeat для отладки
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      Serial.printf("[MEASURE] Heartbeat: %lu ms, измерений: %lu\n",
                    currentMillis, measurementCount);
      lastHeartbeat = currentMillis;
    }

    // 2. Пропускаем цикл если система не инициализирована
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // 3. Периодическая попытка переподключения датчиков
    if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
      attemptReconnect();
      lastReconnectAttempt = currentMillis;
    }

    // 4. Выходим если критическая ошибка (нет гильзы)
    if (criticalError || !sensors[3].found) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // 5. ЗАПРОС ТЕМПЕРАТУР С ДАТЧИКОВ
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

    // 6. Ждем конверсии (уменьшаем время ожидания для отзывчивости)
    vTaskDelay(pdMS_TO_TICKS(750));

    // 7. ЧТЕНИЕ И ОБРАБОТКА ДАННЫХ С ДАТЧИКОВ
    for (int i = 0; i < 4; i++) {
      if (sensors[i].found) {
        float rawTemp = 0;

        // Чтение с правильной шины
        if (i == 3) {
          rawTemp = sensorsA.getTempC(sensors[i].addr);
        } else {
          rawTemp = sensorsB.getTempC(sensors[i].addr);
        }

        // Обработка ошибок датчика
        if (rawTemp == DEVICE_DISCONNECTED_C || !isValidTemperature(rawTemp)) {
          sensors[i].temp = TEMP_NO_DATA;
          safeUpdateSystemData(i, TEMP_NO_DATA, 0.0f);
          sensors[i].lostTimer++;

          // Помечаем датчик как потерянный после 10 неудачных попыток
          if (sensors[i].lostTimer > 10) {
            sensors[i].found = false;
            if (i == 3) {
              criticalError = true;
              systemInitialized = false;
            }
            Serial.printf("⚠️  %s помечен как потерянный\n", sensorNames[i]);
          }
        } else {
          // Нормальное чтение - фильтрация и обновление
          sensors[i].lostTimer = 0;
          sensors[i].temp = filterValue(i, rawTemp);

          // Расчет дельты раз в DELTA_CALC_INTERVAL
          if (currentMillis - lastDeltaTime > DELTA_CALC_INTERVAL) {
            if (lastTemps[i] != 0) {
              float delta = sensors[i].temp - lastTemps[i];
              safeUpdateSystemData(i, sensors[i].temp, delta);
            } else {
              safeUpdateSystemData(i, sensors[i].temp, 0.0f);
            }
            lastTemps[i] = sensors[i].temp;
          } else {
            // Используем последнюю известную дельту
            safeUpdateSystemData(i, sensors[i].temp, sysData.deltas[i]);
          }
        }
      } else {
        // Датчик не найден - отправляем код ошибки
        if (i == 3) {
          safeUpdateSystemData(i, TEMP_CRITICAL_LOST, 0.0f);
          criticalError = true;
          systemInitialized = false;
        } else {
          safeUpdateSystemData(i, TEMP_SENSOR_LOST, 0.0f);
        }
      }
    }

    // 8. Сброс таймера дельт
    if (currentMillis - lastDeltaTime > DELTA_CALC_INTERVAL) {
      lastDeltaTime = currentMillis;
    }

    // 9. ОТПРАВКА ДАННЫХ В ОЧЕРЕДЬ ДЛЯ ДИСПЛЕЯ
    if (dataQueue != NULL) {
      SystemData_t dataToSend;
      safeReadSystemData(&dataToSend);

      // НЕБЛОКИРУЮЩАЯ отправка - если очередь полна, старые данные теряются
      if (xQueueSend(dataQueue, &dataToSend, 0) != pdTRUE) {
        static uint32_t lastQueueError = 0;
        if (currentMillis - lastQueueError > 5000) {
          Serial.println("⚠️  Очередь данных переполнена (данные потеряны)");
          lastQueueError = currentMillis;
        }
      }
    }

    // 10. Обновление таймера стабилизации для MODE1
    if (sysData.mode == 0 && sensors[3].found) {
      float guildTemp = sysData.temps[3];
      mode1_update_stabilization_timer(guildTemp);
    }

    // 11. ТОЧНОЕ ВРЕМЯ ЦИКЛА
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(MEASURE_INTERVAL));
  }
}

// ============================================================================
// ЗАДАЧА ДИСПЛЕЯ (ПЕРЕПИСАНА С НУЛЯ)
// ============================================================================
void taskDisplay(void* pv) {
  SystemData_t displayData;
  uint32_t lastUpdateTime = 0;
  uint32_t lastHeartbeat = 0;
  uint32_t lastStackCheck = 0;
  uint32_t displayUpdates = 0;
  uint32_t lastDisplayMode = 0xFF;

  Serial.println("🖥️  Задача дисплея запущена");

  while (1) {
    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());

    // 1. HEARTBEAT - для обнаружения зависаний
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
      Serial.printf("[DISPLAY] Heartbeat: %lu ms, обновлений: %lu, стек: %u, режим: %d\n",
                    currentMillis, displayUpdates, stackFree * 4, sysData.mode);
      lastHeartbeat = currentMillis;
    }

    // 2. ПРОВЕРКА СТЕКА (раз в 5 минут)
    if (currentMillis - lastStackCheck > STACK_CHECK_INTERVAL) {
      UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
      Serial.printf("[DISPLAY] Свободно стека: %u байт\n", stackFree * 4);
      lastStackCheck = currentMillis;

      // Предупреждение если мало стека
      if (stackFree < 100) {
        Serial.println("⚠️  [DISPLAY] ВНИМАНИЕ: Мало свободного стека!");
      }
    }

    // 3. ПРОВЕРКА ИНИЦИАЛИЗАЦИИ СИСТЕМЫ
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // 4. ОБНОВЛЕНИЕ ФЛАГА КРИТИЧЕСКОЙ ОШИБКИ
    criticalError = !sensors[3].found;

    // 5. ПОЛУЧЕНИЕ ДАННЫХ ИЗ ОЧЕРЕДИ (с таймаутом 100 мс)
    bool newDataReceived = false;
    if (dataQueue != NULL) {
      if (xQueueReceive(dataQueue, &displayData, pdMS_TO_TICKS(100)) == pdTRUE) {
        newDataReceived = true;
        displayUpdates++;

        // 6. БЕЗОПАСНОЕ ОБНОВЛЕНИЕ СИСТЕМНЫХ ДАННЫХ
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
          // Сохраняем старый режим для обнаружения смены
          uint8_t oldMode = sysData.mode;

          // Копируем ВСЕ данные из очереди
          sysData.mode = displayData.mode;
          sysData.needsRedraw = displayData.needsRedraw;
          memcpy(sysData.temps, displayData.temps, sizeof(float) * 4);
          memcpy(sysData.deltas, displayData.deltas, sizeof(float) * 4);

          // Обнаружение смены режима
          if (sysData.mode != oldMode) {
            Serial.printf("[DISPLAY] Смена режима: %d -> %d\n",
                          oldMode, sysData.mode);
            lastDisplayMode = sysData.mode;
            forceDisplayRedraw = true;
          }

          xSemaphoreGive(dataMutex);
        }
      }
    }

    // 7. ПЕРИОДИЧЕСКОЕ ОБНОВЛЕНИЕ ДИСПЛЕЯ
    // Обновляем если: пришли новые данные ИЛИ прошло достаточно времени
    if (newDataReceived || (currentMillis - lastUpdateTime >= DISPLAY_UPDATE_MS)) {

      // 7.1. ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ (только для MODE2)
      if (sysData.mode == 1 && sensors[3].found && guildBaseTemp != 0.0f) {
        // Берем мьютекс для безопасного чтения температуры гильзы
        float currentGuildTemp = 0.0f;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          currentGuildTemp = sysData.temps[3];
          xSemaphoreGive(dataMutex);
        }

        // Обновляем цветовое состояние если температура валидна
        if (isValidTemperature(currentGuildTemp)) {
          mode2_update_color_state(currentGuildTemp);
        }
      }

      // 7.2. ВЫБОР И ВЫЗОВ ФУНКЦИИ ОТРИСОВКИ
      // НЕ берем мьютекс здесь - функции отрисовки сами работают с sysData
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

      lastUpdateTime = currentMillis;
    }

    // 8. КОРОТКАЯ ПАУЗА ДЛЯ ДРУГИХ ЗАДАЧ
    // 20 мс = 50 FPS максимум, но реально обновляем по DISPLAY_UPDATE_MS
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ============================================================================
// ЗАДАЧА SERIAL (С УЛУЧШЕНИЯМИ)
// ============================================================================
void taskSerial(void* pv) {
  uint32_t lastHeartbeat = 0;
  uint32_t commandCount = 0;

  Serial.println("📟 Задача Serial запущена");

  while (1) {
    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());

    // Heartbeat для Serial
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      Serial.printf("[SERIAL] Heartbeat: %lu ms, команд: %lu\n",
                    currentMillis, commandCount);
      lastHeartbeat = currentMillis;
    }

    // Обработка ввода
    serial_handle_input();
    commandCount++;

    // Пауза между проверками (50 мс = 20 проверок в секунду)
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================================
// СОЗДАНИЕ ЗАДАЧ FREERTOS
// ============================================================================
void create_rtos_tasks() {
  Serial.println("\n" + String(50, '='));
  Serial.println("СОЗДАНИЕ ЗАДАЧ FREERTOS (СТАБИЛЬНАЯ ВЕРСИЯ)");
  Serial.println(String(50, '='));

  // 1. Задача измерений (ВЫСОКИЙ приоритет - должна работать точно по времени)
  if (xTaskCreatePinnedToCore(
        taskMeasure,
        "MeasureTask",
        8192,  // 8KB стека
        NULL,
        3,  // Приоритет 3 (высокий)
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу измерений!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА: MeasureTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("✅ Задача измерений создана (приоритет 3, ядро 1, стек 8KB)");

  // 2. Задача дисплея (СРЕДНИЙ приоритет)
  if (xTaskCreatePinnedToCore(
        taskDisplay,
        "DisplayTask",
        12288,  // 12KB стека (много из-за буферов дисплея)
        NULL,
        2,  // Приоритет 2 (средний)
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу дисплея!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА: DisplayTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("✅ Задача дисплея создана (приоритет 2, ядро 1, стек 12KB)");

  // 3. Задача Serial (НИЗКИЙ приоритет)
  if (xTaskCreatePinnedToCore(
        taskSerial,
        "SerialTask",
        4096,  // 4KB стека
        NULL,
        1,  // Приоритет 1 (низкий)
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу Serial!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА: SerialTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("✅ Задача Serial создана (приоритет 1, ядро 1, стек 4KB)");

  // 4. ИНФОРМАЦИЯ О СИСТЕМЕ
  Serial.println("\n" + String(60, '='));
  Serial.println("✅ СИСТЕМА УСПЕШНО ЗАПУЩЕНА");
  Serial.println(String(60, '='));
  Serial.println("ОСОБЕННОСТИ ЭТОЙ ВЕРСИИ:");
  Serial.println("  1. Heartbeat во всех задачах для отладки зависаний");
  Serial.println("  2. Проверка свободного стека каждые 5 минут");
  Serial.println("  3. Мьютекс dataMutex используется КОНСИСТЕНТНО");
  Serial.println("  4. Оптимизировано время удержания мьютекса");
  Serial.println("  5. Неблокирующая отправка в очередь (старые данные теряются)");
  Serial.println("  6. Отдельное обновление цветового состояния");
  Serial.println(String(60, '='));

  // 5. ПРОВЕРКА СТЕКА ОСНОВНОЙ ЗАДАЧИ
  vTaskDelay(pdMS_TO_TICKS(2000));  // Ждем стабилизации
  UBaseType_t mainStack = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("[INIT] Стек основной задачи: %u слов (%u байт)\n",
                mainStack, mainStack * 4);

  if (mainStack < 200) {
    Serial.println("⚠️  ВНИМАНИЕ: Мало стека в основной задаче!");
  }

  Serial.println("\n🔥 Система готова к работе!");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println(String(60, '=') + "\n");
}