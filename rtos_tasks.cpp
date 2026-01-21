#include "rtos_tasks.h"
#include "encoder_engine.h" // ДОБАВЛЕНО: новый модуль энкодера

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ ДЛЯ ОТЛАДКИ
// ============================================================================
#define HEARTBEAT_INTERVAL 30000     // 30 секунд
#define STACK_CHECK_INTERVAL 300000  // 5 минут
#define ENCODER_POLL_INTERVAL 10     // 10 мс - частота опроса энкодера
#define INACTIVITY_TIMEOUT 30000     // 30 сек - таймаут возврата в главный экран

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ ДИСПЛЕЯ (только в этом файле)
// ============================================================================
// static uint8_t systemState = 0;           // 0 = STATE_MAIN, 1 = STATE_MODE
static uint8_t selectedModeIndex = 0;     // 0 = MODE1, 1 = MODE2 (для STATE_MODE)
static uint32_t lastUserActivity = 0;     // Время последней активности

// ============================================================================
// ЗАДАЧА ЭНКОДЕРА (НОВАЯ ЗАДАЧА)
// ============================================================================
void taskEncoder(void* pv) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  
  Serial.println("🎛️  Задача энкодера запущена");

  while (1) {
    // 1. ОПРОС ЭНКОДЕРА
    EncoderEvent_t event = encoder_tick();
    
    // 2. ЕСЛИ ЕСТЬ СОБЫТИЕ - ОТПРАВЛЯЕМ В ОЧЕРЕДЬ
    if (event != EVENT_NONE && eventQueue != NULL) {
      // НЕБЛОКИРУЮЩАЯ отправка (0 тиков ожидания)
      // Если очередь полна - событие теряется (маловероятно при частоте 100 Гц)
      if (xQueueSend(eventQueue, &event, 0) != pdTRUE) {
        static uint32_t lastQueueError = 0;
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
        if (now - lastQueueError > 5000) {
          Serial.println("⚠️  [ENCODER] Очередь событий переполнена");
          lastQueueError = now;
        }
      }
    }
    
    // 3. ТОЧНЫЙ ИНТЕРВАЛ ОПРОСА (100 Гц = каждые 10 мс)
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(ENCODER_POLL_INTERVAL));
  }
}

// ============================================================================
// ЗАДАЧА ИЗМЕРЕНИЙ (БЕЗ ИЗМЕНЕНИЙ)
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
// ЗАДАЧА ДИСПЛЕЯ (С ДОБАВЛЕНИЕМ ОБРАБОТКИ ЭНКОДЕРА)
// ============================================================================
void taskDisplay(void* pv) {
  SystemData_t displayData;
  uint32_t lastUpdateTime = 0;
  uint32_t lastHeartbeat = 0;
  uint32_t lastStackCheck = 0;
  uint32_t displayUpdates = 0;
  uint32_t lastDisplayMode = 0xFF;

  Serial.println("🖥️  Задача дисплея запущена");

  // ИНИЦИАЛИЗАЦИЯ ТАЙМЕРА НЕАКТИВНОСТИ
  lastUserActivity = pdTICKS_TO_MS(xTaskGetTickCount());

  while (1) {
    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());

    // 1. HEARTBEAT (для отладки)
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
      Serial.printf("[DISPLAY] Heartbeat: стейт=%d, выбор=%d, неактивность=%lu сек\n",
                    systemState, selectedModeIndex, 
                    (currentMillis - lastUserActivity) / 1000);
      lastHeartbeat = currentMillis;
    }

    // 2. ПРОВЕРКА ТАЙМАУТА НЕАКТИВНОСТИ (30 секунд)
    if (systemState != 0 && (currentMillis - lastUserActivity > INACTIVITY_TIMEOUT)) {
      Serial.println("[DISPLAY] Таймаут неактивности - возврат в главный экран");
      systemState = 0; // Возвращаемся в STATE_MAIN
      forceDisplayRedraw = true; // Запускаем полную перерисовку
      lastUserActivity = currentMillis; // Сбрасываем таймер
    }

    // 3. ПРОВЕРКА ИНИЦИАЛИЗАЦИИ СИСТЕМЫ (без изменений)
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // 4. ОБНОВЛЕНИЕ ФЛАГА КРИТИЧЕСКОЙ ОШИБКИ (без изменений)
    criticalError = !sensors[3].found;

    // 5. ОБРАБОТКА СОБЫТИЙ ЭНКОДЕРА (НОВАЯ СЕКЦИЯ)
    EncoderEvent_t encoderEvent;
    if (eventQueue != NULL) {
      // Читаем событие из очереди с таймаутом 0 (неблокирующий режим)
      while (xQueueReceive(eventQueue, &encoderEvent, 0) == pdTRUE) {
        // Сбрасываем таймер неактивности при ЛЮБОМ событии
        lastUserActivity = currentMillis;
        
        // ОБРАБОТКА СОБЫТИЙ В ЗАВИСИМОСТИ ОТ ТЕКУЩЕГО СОСТОЯНИЯ
        switch (systemState) {
          case 0: // STATE_MAIN
            if (encoderEvent == EVENT_BUTTON_CLICK) {
              Serial.println("[DISPLAY] Короткое нажатие -> переход в STATE_MODE");
              systemState = 1; // Переходим в режим выбора
              selectedModeIndex = sysData.mode; // Устанавливаем текущий режим как выбранный
              forceDisplayRedraw = true; // Требуем перерисовку
            }
            break;
            
          case 1: // STATE_MODE
            switch (encoderEvent) {
              case EVENT_BUTTON_CLICK:
                Serial.println("[DISPLAY] Короткое нажатие -> возврат в STATE_MAIN");
                systemState = 0; // Возвращаемся в главный экран
                forceDisplayRedraw = true;
                break;
                
              case EVENT_BUTTON_DOUBLE:
                Serial.printf("[DISPLAY] Двойное нажатие -> применение режима %d\n", selectedModeIndex);
                // Применяем выбранный режим через существующую функцию
                resetDisplayState(selectedModeIndex);
                // Возвращаемся в главный экран
                systemState = 0;
                forceDisplayRedraw = true;
                break;
                
              case EVENT_ENCODER_LEFT:
                Serial.println("[DISPLAY] Поворот влево -> выбор MODE1");
                selectedModeIndex = 0; // MODE1
                forceDisplayRedraw = true;
                break;
                
              case EVENT_ENCODER_RIGHT:
                Serial.println("[DISPLAY] Поворот вправо -> выбор MODE2");
                selectedModeIndex = 1; // MODE2
                forceDisplayRedraw = true;
                break;
                
              default:
                // Другие события игнорируем в этом состоянии
                break;
            }
            break;
        }
      }
    }

    // 6. ПОЛУЧЕНИЕ ДАННЫХ ИЗ ОЧЕРЕДИ ТЕМПЕРАТУР (без изменений)
    bool newDataReceived = false;
    if (dataQueue != NULL) {
      if (xQueueReceive(dataQueue, &displayData, pdMS_TO_TICKS(100)) == pdTRUE) {
        newDataReceived = true;
        displayUpdates++;
        
        // ... (обработка данных температуры без изменений) ...
      }
    }

    // 7. ВЫБОР ФУНКЦИИ ОТРИСОВКИ В ЗАВИСИМОСТИ ОТ СОСТОЯНИЯ
    if (newDataReceived || (currentMillis - lastUpdateTime >= DISPLAY_UPDATE_MS) || forceDisplayRedraw) {
      
      // ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ (для MODE2, без изменений)
      if (sysData.mode == 1 && sensors[3].found && guildBaseTemp != 0.0f) {
        float currentGuildTemp = 0.0f;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          currentGuildTemp = sysData.temps[3];
          xSemaphoreGive(dataMutex);
        }
        if (isValidTemperature(currentGuildTemp)) {
          mode2_update_color_state(currentGuildTemp);
        }
      }
      
      // ВЫБОР ЭКРАНА ДЛЯ ОТРИСОВКИ
      switch (systemState) {
        case 0: // STATE_MAIN - главный экран с температурами
          if (sysData.mode == 0) {
            updateDisplayMODE1();
          } else {
            switch (guildColorState) {
              case 0: updateDisplayMODE2_GREEN(); break;
              case 1: updateDisplayMODE2_YELLOW(); break;
              case 2: updateDisplayMODE2_RED(); break;
              default: updateDisplayMODE1(); break;
            }
          }
          break;
          
        case 1: // STATE_MODE - экран выбора режима
          // ЗАГЛУШКА: временно просто очищаем экран и пишем текст
          // TODO: реализовать полноценную отрисовку с курсором
          tft.fillScreen(COLOR_BLACK);
          tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
          tft.setTextFont(FONT_BIG);
          tft.setCursor(50, 50);
          tft.printf("Выбор режима: %s", selectedModeIndex == 0 ? "MODE1" : "MODE2");
          tft.setCursor(50, 100);
          tft.printf("Текущий: %s", sysData.mode == 0 ? "MODE1" : "MODE2");
          tft.setCursor(50, 150);
          tft.print("Кнопка - назад, 2xКнопка - применить");
          break;
          
        default:
          // Если по какой-то причине неизвестное состояние - показываем главный экран
          systemState = 0;
          updateDisplayMODE1();
          break;
      }
      
      lastUpdateTime = currentMillis;
      forceDisplayRedraw = false; // Сбрасываем флаг после отрисовки
    }

    // 8. КОРОТКАЯ ПАУЗА ДЛЯ ДРУГИХ ЗАДАЧ (без изменений)
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
// СОЗДАНИЕ ЗАДАЧ FREERTOS (С ДОБАВЛЕНИЕМ ЗАДАЧИ ЭНКОДЕРА)
// ============================================================================
void create_rtos_tasks() {
  Serial.println("\n" + String(50, '='));
  Serial.println("СОЗДАНИЕ ЗАДАЧ FREERTOS (ВЕРСИЯ С ЭНКОДЕРОМ)");
  Serial.println(String(50, '='));

  // 1. ЗАДАЧА ЭНКОДЕРА (НОВАЯ ЗАДАЧА, высокий приоритет)
  if (xTaskCreatePinnedToCore(
        taskEncoder,
        "EncoderTask",
        4096,  // 4KB стека достаточно
        NULL,
        4,  // Высокий приоритет (4) для быстрого отклика
        NULL,
        1)  // Ядро 1 (как и другие задачи)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу энкодера!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА: EncoderTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // ИСПРАВЛЕНО: замена delay
  }
  Serial.println("✅ Задача энкодера создана (приоритет 4, ядро 1, стек 4KB)");

  // 2. Задача измерений (ВЫСОКИЙ приоритет - должна работать точно по времени)
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
    tft.setCursor(20, 120);
    tft.print("ОШИБКА: MeasureTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // ИСПРАВЛЕНО: замена delay
  }
  Serial.println("✅ Задача измерений создана (приоритет 3, ядро 1, стек 8KB)");

  // 3. Задача дисплея (СРЕДНИЙ приоритет)
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
    tft.setCursor(20, 140);
    tft.print("ОШИБКА: DisplayTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // ИСПРАВЛЕНО: замена delay
  }
  Serial.println("✅ Задача дисплея создана (приоритет 2, ядро 1, стек 12KB)");

  // 4. Задача Serial (НИЗКИЙ приоритет)
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
    tft.setCursor(20, 160);
    tft.print("ОШИБКА: SerialTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // ИСПРАВЛЕНО: замена delay
  }
  Serial.println("✅ Задача Serial создана (приоритет 1, ядро 1, стек 4KB)");

  Serial.println(String(50, '='));
  Serial.println("✅ ВСЕ ЗАДАЧИ УСПЕШНО СОЗДАНЫ");
  Serial.println(String(50, '=') + "\n");
}