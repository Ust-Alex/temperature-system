/**
 * ============================================================================
 * ФАЙЛ: rtos_tasks.cpp
 * ОСНОВНОЙ ФАЙЛ ЗАДАЧ FREERTOS ДЛЯ СИСТЕМЫ МОНИТОРИНГА ТЕМПЕРАТУР
 * 
 * ВЕРСИЯ: 4.2 (ДОБАВЛЕН ВЫЗОВ RESETDISPLAYSTATE ПРИ ПЕРЕКЛЮЧЕНИИ РЕЖИМОВ)
 * 
 * ОСОБЕННОСТИ:
 * 1. Четыре задачи FreeRTOS: энкодер, измерения, дисплей, serial
 * 2. Механизм очереди событий для энкодера
 * 3. Машина состояний интерфейса (STATE_MAIN / STATE_MODE)
 * 4. Автоматический возврат в главный экран по таймауту (30 сек)
 * 5. Heartbeat-сообщения для отладки
 * ============================================================================
 */

#include "rtos_tasks.h"
#include "measurement_task.h"
#include "sensors.h"
#include "encoder_engine.h"  // Модуль для работы с энкодером
#include "calibration_simple.h"
#include "display_engine.h"  // ДОБАВЛЕНО для resetDisplayState

extern float calibrationOffsets[4];  // Массив offset'ов
extern int referenceSensor;          // Индекс эталонного датчика
extern bool calibrationEnabled;      // Флаг включения калибровки

// ============================================================================
// КОНФИГУРАЦИОННЫЕ КОНСТАНТЫ (МАКРОСЫ)
// ============================================================================
#define HEARTBEAT_INTERVAL 30000     // Интервал heartbeat-сообщений: 30 секунд
#define STACK_CHECK_INTERVAL 300000  // Проверка свободного стека: каждые 5 минут
#define ENCODER_POLL_INTERVAL 10     // Частота опроса энкодера: 10 мс (100 Гц)
#define INACTIVITY_TIMEOUT 30000     // Таймаут неактивности: 30 секунд

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ ДИСПЛЕЯ (ВИДНЫ ТОЛЬКО В ЭТОМ ФАЙЛЕ)
// ============================================================================

static uint8_t selectedModeIndex = 0;  // Индекс выбранного режима в меню:
                                       // 0 = MODE1 (стабилизация), 1 = MODE2 (рабочий)
static uint32_t lastUserActivity = 0;  // Время последней активности пользователя
                                       // (используется для таймаута возврата в главный экран)

// ============================================================================
// ЗАДАЧА ЭНКОДЕРА
// ============================================================================
void taskEncoder(void* pv) {
  TickType_t lastWakeTime = xTaskGetTickCount();

  Serial.println("🎛️  Задача энкодера запущена");

  while (1) {
    EncoderEvent_t event = encoder_tick();

    if (event != EVENT_NONE) {
      Serial.printf("[ENCODER] Событие: %d\n", event);
    }

    if (event != EVENT_NONE && eventQueue != NULL) {
      if (xQueueSend(eventQueue, &event, 0) != pdTRUE) {
        static uint32_t lastQueueError = 0;
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

        if (now - lastQueueError > 5000) {
          Serial.println("⚠️  [ENCODER] Очередь событий переполнена");
          lastQueueError = now;
        }
      }
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(ENCODER_POLL_INTERVAL));
  }
}

// ============================================================================
// ЗАДАЧА SERIAL ИНТЕРФЕЙСА
// ============================================================================
void taskSerial(void* pv) {
  uint32_t lastHeartbeat = 0;
  uint32_t commandCount = 0;

  Serial.println("📟 Задача Serial запущена");
  Serial.println("🎛️  Введите HELP для списка команд");

  while (1) {
    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());

    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      // Serial.printf("[SERIAL] Heartbeat: %lu ms, команд: %lu\n",
      //               currentMillis, commandCount);
      lastHeartbeat = currentMillis;
    }

    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      command.toUpperCase();
      commandCount++;

      Serial.printf("> %s\n", command.c_str());

      // --------------------------------------------
      // СИСТЕМНЫЕ КОМАНДЫ
      // --------------------------------------------
      if (command == "HELP" || command == "?") {
        Serial.println("\n" + String(60, '='));
        Serial.println("📋 СПИСОК КОМАНД");
        Serial.println(String(60, '='));

        Serial.println("\n🔧 СИСТЕМНЫЕ КОМАНДЫ:");
        Serial.println("  HELP     - Этот список команд");
        Serial.println("  STATUS   - Полный статус системы");
        Serial.println("  FIND     - Принудительный поиск датчиков");
        Serial.println("  REBOOT   - Перезагрузка системы");

        Serial.println("\n🎛️  РЕЖИМЫ РАБОТЫ:");
        Serial.println("  MODE1    - Режим стабилизации (синий)");
        Serial.println("  MODE2    - Рабочий режим (зелёный/жёлтый/красный)");

        Serial.println("\n📊 КАЛИБРОВКА ДАТЧИКОВ:");
        Serial.println("  CALIB SHOW   - Показать коэффициенты калибровки");
        Serial.println("  CALIB AUTO   - Автокалибровка относительно эталона");
        Serial.println("  CALIB ON     - Включить калибровку");
        Serial.println("  CALIB OFF    - Выключить калибровку");
        Serial.println("  CALIB RESET  - Сбросить все offset к 0");
        Serial.println("  CALIB REF N  - Сделать датчик N эталоном (0-3)");
        Serial.println("  CALIB SET N X - Установить offset X для датчика N");
        Serial.println("                Пример: CALIB SET 3 -0.5");

        Serial.println("\n⚙️  ДИАГНОСТИКА:");
        Serial.println("  DEBUG ON  - Включить отладочный вывод");
        Serial.println("  DEBUG OFF - Выключить отладочный вывод");

        Serial.println(String(60, '='));
      }

      else if (command == "STATUS") {
        Serial.println("\n" + String(50, '='));
        Serial.println("          СТАТУС СИСТЕМЫ");
        Serial.println(String(50, '='));

        Serial.printf("Режим работы: %s\n",
                      sysData.mode == 0 ? "СТАБИЛИЗАЦИЯ (MODE1)" : "РАБОЧИЙ (MODE2)");
        Serial.printf("Система инициализирована: %s\n",
                      systemInitialized ? "ДА" : "НЕТ");
        Serial.printf("Критическая ошибка: %s\n",
                      criticalError ? "ДА" : "НЕТ");
        Serial.printf("Базовая темп. гильзы: %.2f°C\n", guildBaseTemp);
        Serial.printf("Флаг перерисовки: %s\n",
                      forceDisplayRedraw ? "ДА" : "НЕТ");

        Serial.println("\n--- СОСТОЯНИЕ ДАТЧИКОВ ---");
        for (int i = 0; i < 4; i++) {
          Serial.printf("  [%d] %s: ", i, sensorNames[i]);

          if (sensors[i].found) {
            float temp = sysData.temps[i];
            float delta = sysData.deltas[i];

            if (temp == TEMP_NO_DATA) {
              Serial.print("⚠️  Нет данных");
            } else if (temp == TEMP_SENSOR_LOST) {
              Serial.print("❌ Потерян");
            } else if (temp == TEMP_CRITICAL_LOST) {
              Serial.print("🔥 КРИТИЧЕСКАЯ ПОТЕРЯ");
            } else {
              Serial.printf("✅ Найден, %.2f°C, Δ(2с): %+.2f°C", temp, delta);
            }
          } else {
            Serial.print("❌ Не найден");
          }
          Serial.println();
        }

        Serial.println("\n--- ЗАДАЧИ FREERTOS ---");
        Serial.printf("  Очередь данных: %s\n",
                      dataQueue ? "Создана" : "Отсутствует");
        if (dataQueue) {
          Serial.printf("  Свободное место в очереди: %d\n",
                        uxQueueSpacesAvailable(dataQueue));
        }
        Serial.printf("  Мьютекс данных: %s\n",
                      dataMutex ? "Создан" : "Отсутствует");
        Serial.println(String(50, '='));
      }

      else if (command == "FIND") {
        Serial.println("🔍 Принудительный поиск датчиков...");
        sensors_scan_all();
        forceDisplayRedraw = true;
      }

      else if (command == "REBOOT") {
        Serial.println("🔄 Перезагрузка системы...");
        delay(1000);
        ESP.restart();
      }

      // --------------------------------------------
      // КОМАНДЫ РЕЖИМОВ РАБОТЫ - ИСПРАВЛЕНО
      // --------------------------------------------
      else if (command == "MODE1") {
        resetDisplayState(0);  // Вызываем полный сброс с новым режимом
        Serial.println("🔵 Режим установлен: MODE1 (СТАБИЛИЗАЦИЯ)");
      }

      else if (command == "MODE2") {
        resetDisplayState(1);  // Вызываем полный сброс с новым режимом
        Serial.println("🟢 Режим установлен: MODE2 (РАБОЧИЙ)");
      }

      // --------------------------------------------
      // КОМАНДЫ КАЛИБРОВКИ
      // --------------------------------------------
      else if (command == "CALIB SHOW") {
        printCalibrationStatus();
      }

      else if (command == "CALIB AUTO") {
        autoCalibrateAllSensors();
      }

      else if (command == "CALIB ON") {
        toggleCalibration(true);
      }

      else if (command == "CALIB OFF") {
        toggleCalibration(false);
      }

      else if (command == "CALIB RESET") {
        for (int i = 0; i < 4; i++) {
          calibrationOffsets[i] = 0.0f;
        }
        saveOffsetsToEEPROM();
        Serial.println("[CALIB] ✅ Все offset сброшены в 0");
      }

      else if (command.startsWith("CALIB REF ")) {
        int idx = command.substring(10).toInt();
        if (idx >= 0 && idx < 4) {
          setReferenceSensor(idx);
        } else {
          Serial.println("[CALIB] ❌ Неверный индекс датчика (0-3)");
        }
      }

      else if (command.startsWith("CALIB SET ")) {
        int firstSpace = 9;
        int secondSpace = command.indexOf(' ', firstSpace + 1);

        if (secondSpace > 0) {
          int idx = command.substring(firstSpace, secondSpace).toInt();
          float offset = command.substring(secondSpace + 1).toFloat();

          if (idx >= 0 && idx < 4) {
            setManualOffset(idx, offset);
          } else {
            Serial.println("[CALIB] ❌ Неверный индекс датчика (0-3)");
          }
        } else {
          Serial.println("[CALIB] ❌ Формат: CALIB SET [0-3] [offset]");
          Serial.println("       Пример: CALIB SET 3 -0.5");
        }
      }

      // --------------------------------------------
      // КОМАНДЫ ДИАГНОСТИКИ
      // --------------------------------------------
      else if (command == "DEBUG ON") {
        Serial.println("🐛 Отладочный вывод ВКЛЮЧЁН");
      }

      else if (command == "DEBUG OFF") {
        Serial.println("🐛 Отладочный вывод ВЫКЛЮЧЕН");
      }

      else {
        Serial.println("❌ Неизвестная команда. Введите HELP для списка.");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================================
// ФУНКЦИИ УПРАВЛЕНИЯ FREERTOS
// ============================================================================

void initFreeRTOSObjects() {
  Serial.println("[RTOS] Создание объектов FreeRTOS...");

  dataQueue = xQueueCreate(5, sizeof(SystemData_t));
  if (dataQueue == NULL) {
    Serial.println("❌ Ошибка создания очереди данных!");
  } else {
    Serial.println("   ✅ Очередь данных создана");
  }

  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("❌ Ошибка создания мьютекса!");
  } else {
    Serial.println("   ✅ Мьютекс создан");
  }

  eventQueue = xQueueCreate(10, sizeof(uint8_t));
  if (eventQueue == NULL) {
    Serial.println("❌ Ошибка создания очереди событий!");
  } else {
    Serial.println("   ✅ Очередь событий создана (10 событий)");
  }
}

// ============================================================================
// ФУНКЦИЯ СОЗДАНИЯ ВСЕХ ЗАДАЧ
// ============================================================================

void create_rtos_tasks() {
  Serial.println("\n[RTOS] Создание задач...");

  xTaskCreate(taskEncoder, "Encoder", 2048, NULL, 4, NULL);
  xTaskCreate(taskMeasure, "Measure", 4096, NULL, 3, NULL);
  xTaskCreate(taskDisplay, "Display", 4096, NULL, 2, NULL);
  xTaskCreate(taskSerial, "Serial", 3072, NULL, 1, NULL);

  Serial.println("[RTOS] Все задачи созданы");
}