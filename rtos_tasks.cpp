/** * ФАЙЛ: rtos_tasks.cpp
 * ОСНОВНОЙ ФАЙЛ ЗАДАЧ FREERTOS ДЛЯ СИСТЕМЫ МОНИТОРИНГА ТЕМПЕРАТУР
 * 
 * ВЕРСИЯ: 4.0 (С ИНТЕГРАЦИЕЙ ЭНКОДЕРА)
 * ДАТА: [Текущая дата]
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
#include "encoder_engine.h"  // Модуль для работы с энкодером (новая библиотека)
#include "calibration_simple.h"

extern float calibrationOffsets[4];    // Массив offset'ов
extern int referenceSensor;           // Индекс эталонного датчика  
extern bool calibrationEnabled;       // Флаг включения калибровки

// ============================================================================
// КОНФИГУРАЦИОННЫЕ КОНСТАНТЫ (МАКРОСЫ)
// ============================================================================
// #define HEARTBEAT_INTERVAL 30000     // Интервал heartbeat-сообщений: 30 секунд
#define HEARTBEAT_INTERVAL 30000      // Интервал heartbeat-сообщений: 30 секунд
#define STACK_CHECK_INTERVAL 300000  // Проверка свободного стека: каждые 5 минут
#define ENCODER_POLL_INTERVAL 10     // Частота опроса энкодера: 10 мс (100 Гц)
#define INACTIVITY_TIMEOUT 30000     // Таймаут неактивности: 30 секунд

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ ДИСПЛЕЯ (ВИДНЫ ТОЛЬКО В ЭТОМ ФАЙЛЕ)
// ============================================================================

// Примечание: 'systemState' теперь ГЛОБАЛЬНАЯ переменная (объявлена в temperature_system.ino)
// и используется для управления состоянием интерфейса (0 = главный экран, 1 = выбор режима)

static uint8_t selectedModeIndex = 0;  // Индекс выбранного режима в меню:
                                       // 0 = MODE1 (стабилизация), 1 = MODE2 (рабочий)
static uint32_t lastUserActivity = 0;  // Время последней активности пользователя
                                       // (используется для таймаута возврата в главный экран)

// ============================================================================
// ЗАДАЧА ЭНКОДЕРА (НОВАЯ ЗАДАЧА, ДОБАВЛЕНА ДЛЯ УПРАВЛЕНИЯ ЧЕРЕЗ ЭНКОДЕР)
// ============================================================================
void taskEncoder(void* pv) {
  TickType_t lastWakeTime = xTaskGetTickCount();  // Для точного временного цикла

  Serial.println("🎛️  Задача энкодера запущена");

  while (1) {  // Бесконечный цикл - требование FreeRTOS для задач
    // 1. ОПРОС ЭНКОДЕРА
    // Функция encoder_tick() опрашивает аппаратный энкодер и возвращает событие
    EncoderEvent_t event = encoder_tick();

    // 2. ОТПРАВКА СОБЫТИЯ В ОЧЕРЕДЬ (ЕСЛИ ЕСТЬ)
    if (event != EVENT_NONE && eventQueue != NULL) {
      // Неблокирующая отправка (0 тиков ожидания)
      // Если очередь полна - событие теряется (лучше потерять событие, чем заблокировать задачу)
      if (xQueueSend(eventQueue, &event, 0) != pdTRUE) {
        static uint32_t lastQueueError = 0;
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

        // Логируем ошибку переполнения очереди не чаще чем раз в 5 секунд
        if (now - lastQueueError > 5000) {
          Serial.println("⚠️  [ENCODER] Очередь событий переполнена");
          lastQueueError = now;
        }
      }
    }

    // 3. ТОЧНЫЙ ИНТЕРВАЛ ОПРОСА
    // Используем vTaskDelayUntil для гарантированного интервала 10 мс (100 Гц)
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(ENCODER_POLL_INTERVAL));
  }
}


// ============================================================================
// ЗАДАЧА ДИСПЛЕЯ (ПЕРЕРАБОТАНА ДЛЯ ПОДДЕРЖКИ ЭНКОДЕРА И МАШИНЫ СОСТОЯНИЙ)
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

    // 1. HEARTBEAT ДЛЯ ОТЛАДКИ
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
      // Serial.printf("[DISPLAY] Heartbeat: стейт=%d, выбор=%d, неактивность=%lu сек\n",
      //               systemState, selectedModeIndex,
      //               (currentMillis - lastUserActivity) / 1000);
      lastHeartbeat = currentMillis;
    }

    // 2. ПРОВЕРКА ТАЙМАУТА НЕАКТИВНОСТИ (30 СЕКУНД)
    // Если пользователь неактивен 30 секунд и не в главном экране - возвращаемся
    if (systemState != 0 && (currentMillis - lastUserActivity > INACTIVITY_TIMEOUT)) {
      Serial.println("[DISPLAY] Таймаут неактивности - возврат в главный экран");
      systemState = 0;                   // Возвращаемся в STATE_MAIN
      forceDisplayRedraw = true;         // Требуем полную перерисовку
      lastUserActivity = currentMillis;  // Сбрасываем таймер
    }

    // 3. ПРОВЕРКА ИНИЦИАЛИЗАЦИИ СИСТЕМЫ
    if (!systemInitialized) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // 4. ОБНОВЛЕНИЕ ФЛАГА КРИТИЧЕСКОЙ ОШИБКИ
    criticalError = !sensors[3].found;  // Критическая ошибка = отсутствие гильзы

    // 5. ОБРАБОТКА СОБЫТИЙ ЭНКОДЕРА (НОВАЯ СЕКЦИЯ ДЛЯ УПРАВЛЕНИЯ ИНТЕРФЕЙСОМ)
    EncoderEvent_t encoderEvent;
    if (eventQueue != NULL) {
      // Читаем ВСЕ события из очереди (неблокирующий режим, таймаут 0)
      while (xQueueReceive(eventQueue, &encoderEvent, 0) == pdTRUE) {
        // Сбрасываем таймер неактивности при ЛЮБОМ действии пользователя
        lastUserActivity = currentMillis;

        // ОБРАБОТКА СОБЫТИЙ В ЗАВИСИМОСТИ ОТ ТЕКУЩЕГО СОСТОЯНИЯ
        switch (systemState) {
          case 0:  // STATE_MAIN (ГЛАВНЫЙ ЭКРАН С ТЕМПЕРАТУРАМИ)
            if (encoderEvent == EVENT_BUTTON_CLICK) {
              // КОРОТКОЕ НАЖАТИЕ: переход в экран выбора режима
              Serial.println("[DISPLAY] Короткое нажатие -> переход в STATE_MODE");
              systemState = 1;                   // Меняем состояние
              selectedModeIndex = sysData.mode;  // Устанавливаем текущий режим как выбранный
              forceDisplayRedraw = true;         // Требуем перерисовку
            }
            break;

          case 1:  // STATE_MODE (ЭКРАН ВЫБОРА РЕЖИМА)
            switch (encoderEvent) {
              case EVENT_BUTTON_CLICK:
                // КОРОТКОЕ НАЖАТИЕ: возврат в главный экран без изменений
                Serial.println("[DISPLAY] Короткое нажатие -> возврат в STATE_MAIN");
                systemState = 0;
                forceDisplayRedraw = true;
                break;

              case EVENT_BUTTON_DOUBLE:
                // ДВОЙНОЕ НАЖАТИЕ: применение выбранного режима
                Serial.printf("[DISPLAY] Двойное нажатие -> применение режима %d\n", selectedModeIndex);
                // Применяем выбранный режим через существующую функцию
                resetDisplayState(selectedModeIndex);
                // Возвращаемся в главный экран
                systemState = 0;
                forceDisplayRedraw = true;
                break;

              case EVENT_ENCODER_LEFT:
                // ПОВОРОТ ВЛЕВО: выбор MODE1
                Serial.println("[DISPLAY] Поворот влево -> выбор MODE1");
                selectedModeIndex = 0;  // MODE1
                forceDisplayRedraw = true;
                break;

              case EVENT_ENCODER_RIGHT:
                // ПОВОРОТ ВПРАВО: выбор MODE2
                Serial.println("[DISPLAY] Поворот вправо -> выбор MODE2");
                selectedModeIndex = 1;  // MODE2
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

    // 6. ПОЛУЧЕНИЕ ДАННЫХ ИЗ ОЧЕРЕДИ ТЕМПЕРАТУР
    bool newDataReceived = false;
    if (dataQueue != NULL) {
      // Чтение с таймаутом 100 мс (если нет данных - не блокируемся надолго)
      if (xQueueReceive(dataQueue, &displayData, pdMS_TO_TICKS(100)) == pdTRUE) {
        newDataReceived = true;
        displayUpdates++;

        // 6.1 БЕЗОПАСНОЕ ОБНОВЛЕНИЕ СИСТЕМНЫХ ДАННЫХ (ПОД МЬЮТЕКСОМ)
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
          // Сохраняем старый режим для обнаружения смены
          uint8_t oldMode = sysData.mode;

          // Копируем ВСЕ данные из очереди в глобальную структуру sysData
          sysData.mode = displayData.mode;
          sysData.needsRedraw = displayData.needsRedraw;
          memcpy(sysData.temps, displayData.temps, sizeof(float) * 4);
          memcpy(sysData.deltas, displayData.deltas, sizeof(float) * 4);

          // ОБНАРУЖЕНИЕ СМЕНЫ РЕЖИМА
          if (sysData.mode != oldMode) {
            Serial.printf("[DISPLAY] Смена режима: %d -> %d\n",
                          oldMode, sysData.mode);
            lastDisplayMode = sysData.mode;
            forceDisplayRedraw = true;  // При смене режима нужна полная перерисовка
          }

          xSemaphoreGive(dataMutex);  // ВАЖНО: всегда отпускаем мьютекс
        }
      }
    }

    // 7. ПЕРИОДИЧЕСКОЕ ОБНОВЛЕНИЕ ДИСПЛЕЯ
    // Обновляем если: 1) пришли новые данные, 2) прошло DISPLAY_UPDATE_MS,
    // 3) установлен флаг forceDisplayRedraw (при смене режима или по таймауту)
    if (newDataReceived || (currentMillis - lastUpdateTime >= DISPLAY_UPDATE_MS) || forceDisplayRedraw) {

      // 7.1 ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ (ТОЛЬКО ДЛЯ MODE2)
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

      // 7.2 ВЫБОР И ВЫЗОВ ФУНКЦИИ ОТРИСОВКИ В ЗАВИСИМОСТИ ОТ СОСТОЯНИЯ
      switch (systemState) {
        case 0:  // STATE_MAIN - ГЛАВНЫЙ ЭКРАН С ТЕМПЕРАТУРАМИ
          if (sysData.mode == 0) {
            updateDisplayMODE1();  // Режим стабилизации (синий фон)
          } else {
            // Режим MODE2 с цветовыми состояниями
            switch (guildColorState) {
              case 0:
                updateDisplayMODE2_GREEN();  // Зеленый фон
                break;
              case 1:
                updateDisplayMODE2_YELLOW();  // Желтый фон
                break;
              case 2:
                updateDisplayMODE2_RED();  // Красный фон
                break;
              default:
                updateDisplayMODE1();  // Фолбэк
                break;
            }
          }
          break;

        case 1:  // STATE_MODE - ЭКРАН ВЫБОРА РЕЖИМА
          // ВРЕМЕННАЯ ЗАГЛУШКА: просто очищаем экран и выводим текст
          // TODO: реализовать полноценную отрисовку с курсором и подсветкой
          tft.fillScreen(COLOR_BLACK);
          tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
          // tft.setTextFont(FONT_BIG);
          tft.setTextFont(FONT_DELTA);
          // tft.setTextFont(FONT_SMALL);
          // tft.setCursor(50, 50);
          tft.setCursor(5, 50);
          // tft.printf("Выбор режима: %s", selectedModeIndex == 0 ? "MODE1" : "MODE2");
          tft.printf("Vibor regima: %s", selectedModeIndex == 0 ? "MODE1" : "MODE2");
          // tft.setCursor(50, 100);
          tft.setCursor(5, 100);
          // tft.printf("Текущий: %s", sysData.mode == 0 ? "MODE1" : "MODE2");
          tft.printf("Tekuschiy: %s", sysData.mode == 0 ? "MODE1" : "MODE2");
          // tft.setCursor(50, 150);
          tft.setCursor(5, 150);
          tft.print("Кнопка - назад, 2xКнопка - применить");
          break;

        default:
          // Если по какой-то причине неизвестное состояние - показываем главный экран
          systemState = 0;
          updateDisplayMODE1();
          break;
      }

      lastUpdateTime = currentMillis;
      forceDisplayRedraw = false;  // Сбрасываем флаг после отрисовки
    }

    // 8. КОРОТКАЯ ПАУЗА ДЛЯ ДРУГИХ ЗАДАЧ
    // 20 мс = 50 FPS максимум, реально обновляем по DISPLAY_UPDATE_MS (500 мс)
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ЗАДАЧА SERIAL ИНТЕРФЕЙСА
// ============================================================================
void taskSerial(void* pv) {
  uint32_t lastHeartbeat = 0;
  uint32_t commandCount = 0;

  Serial.println("📟 Задача Serial запущена");
  Serial.println("🎛️  Введите HELP для списка команд");

  while (1) {
    uint32_t currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());

    // HEARTBEAT ДЛЯ ОТЛАДКИ
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      // Serial.printf("[SERIAL] Heartbeat: %lu ms, команд: %lu\n",
      //               currentMillis, commandCount);
      lastHeartbeat = currentMillis;
    }

    // ОБРАБОТКА ВХОДЯЩИХ КОМАНД
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      command.toUpperCase();  // Для единообразия
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
            
            // Проверяем специальные значения
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
        findSensors();
        attemptReconnect();
      }

      else if (command == "REBOOT") {
        Serial.println("🔄 Перезагрузка системы...");
        delay(1000);
        ESP.restart();
      }

      // --------------------------------------------
      // КОМАНДЫ РЕЖИМОВ РАБОТЫ
      // --------------------------------------------
      else if (command == "MODE1") {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          sysData.mode = 0;  // Режим стабилизации
          sysData.needsRedraw = true;
          xSemaphoreGive(dataMutex);
          Serial.println("🔵 Режим установлен: MODE1 (СТАБИЛИЗАЦИЯ)");
          forceDisplayRedraw = true;
        }
      }

      else if (command == "MODE2") {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          sysData.mode = 1;  // Рабочий режим
          sysData.needsRedraw = true;
          xSemaphoreGive(dataMutex);
          Serial.println("🟢 Режим установлен: MODE2 (РАБОЧИЙ)");
          
          // Сохраняем текущую температуру гильзы как базовую
          if (sensors[3].found) {
            guildBaseTemp = sysData.temps[3];
            Serial.printf("   Базовая температура гильзы: %.2f°C\n", guildBaseTemp);
          }
          forceDisplayRedraw = true;
        }
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
        // Формат: CALIB SET [idx] [offset]
        // Пример: CALIB SET 3 -0.5
        int firstSpace = 9; // После "CALIB SET "
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
        // Включить отладочный вывод
        // Здесь можно добавить глобальный флаг debugMode
        Serial.println("🐛 Отладочный вывод ВКЛЮЧЕН");
      }

      else if (command == "DEBUG OFF") {
        // Выключить отладочный вывод
        Serial.println("🐛 Отладочный вывод ВЫКЛЮЧЕН");
      }

      // --------------------------------------------
      // НЕИЗВЕСТНАЯ КОМАНДА
      // --------------------------------------------
      else {
        Serial.printf("❌ Неизвестная команда: %s\n", command.c_str());
        Serial.println("   Введите HELP для списка команд");
      }
    }

    // ЗАДЕРЖКА ДЛЯ ОСВОБОЖДЕНИЯ ЦП
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================================
// СОЗДАНИЕ ЗАДАЧ FREERTOS (ОБНОВЛЕНА ДЛЯ СОЗДАНИЯ ЗАДАЧИ ЭНКОДЕРА)
// ============================================================================
void create_rtos_tasks() {
  Serial.println("\n" + String(50, '='));
  Serial.println("СОЗДАНИЕ ЗАДАЧ FREERTOS (ВЕРСИЯ 4.0 С ЭНКОДЕРОМ)");
  Serial.println(String(50, '='));

  // 1. ЗАДАЧА ЭНКОДЕРА (НОВАЯ ЗАДАЧА, ВЫСОКИЙ ПРИОРИТЕТ ДЛЯ БЫСТРОГО ОТКЛИКА)
  if (xTaskCreatePinnedToCore(
        taskEncoder,    // Функция задачи
        "EncoderTask",  // Имя задачи (для отладки)
        4096,           // Размер стека: 4KB (достаточно для простой задачи)
        NULL,           // Параметры (не используются)
        4,              // Приоритет: 4 (высокий) - важен быстрый отклик на действия
        NULL,           // Дескриптор задачи (не сохраняем)
        1)              // Ядро процессора: 1 (как и все задачи для согласованности)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу энкодера!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА: EncoderTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));  // Аварийная остановка
  }
  Serial.println("✅ Задача энкодера создана (приоритет 4, ядро 1, стек 4KB)");

  // 2. ЗАДАЧА ИЗМЕРЕНИЙ (ВЫСОКИЙ ПРИОРИТЕТ - ДОЛЖНА РАБОТАТЬ ТОЧНО ПО ВРЕМЕНИ)
  if (xTaskCreatePinnedToCore(
        taskMeasure,
        "MeasureTask",
        8192,  // 8KB стека (больше из-за сложных вычислений и буферов)
        NULL,
        3,  // Приоритет 3 (высокий, но ниже энкодера)
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу измерений!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 120);
    tft.print("ОШИБКА: MeasureTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("✅ Задача измерений создана (приоритет 3, ядро 1, стек 8KB)");

  // 3. ЗАДАЧА ДИСПЛЕЯ (СРЕДНИЙ ПРИОРИТЕТ)
  if (xTaskCreatePinnedToCore(
        taskDisplay,
        "DisplayTask",
        12288,  // 12KB стека (много из-за буферов дисплея и графических операций)
        NULL,
        2,  // Приоритет 2 (средний)
        NULL,
        1)
      != pdPASS) {
    Serial.println("❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать задачу дисплея!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 140);
    tft.print("ОШИБКА: DisplayTask");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("✅ Задача дисплея создана (приоритет 2, ядро 1, стек 12KB)");

  // 4. ЗАДАЧА SERIAL (НИЗКИЙ ПРИОРИТЕТ - КОМАНДЫ ПОЛЬЗОВАТЕЛЯ НЕ КРИТИЧНЫ ПО ВРЕМЕНИ)
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
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("✅ Задача Serial создана (приоритет 1, ядро 1, стек 4KB)");

  // 5. ИНФОРМАЦИЯ О СИСТЕМЕ И ЗАВЕРШЕНИЕ ИНИЦИАЛИЗАЦИИ
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
  Serial.println("  7. НОВАЯ ЗАДАЧА: Управление через энкодер с очередью событий");
  Serial.println("  8. НОВАЯ ФУНКЦИЯ: Машина состояний интерфейса (STATE_MAIN/MODE)");
  Serial.println("  9. НОВАЯ ФУНКЦИЯ: Авто-возврат в главный экран по таймауту");
  Serial.println(String(60, '='));

  // 6. ПРОВЕРКА СТЕКА ОСНОВНОЙ ЗАДАЧИ (ДОПОЛНИТЕЛЬНАЯ ДИАГНОСТИКА)
  vTaskDelay(pdMS_TO_TICKS(2000));  // Ждем 2 секунды для стабилизации системы
  UBaseType_t mainStack = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("[INIT] Стек основной задачи: %u слов (%u байт)\n",
                mainStack, mainStack * 4);

  if (mainStack < 200) {
    Serial.println("⚠️  ВНИМАНИЕ: Мало стека в основной задаче!");
  }

  Serial.println("\n🔥 Система готова к работе!");
  Serial.println("🎛️  Используйте энкодер для навигации");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println(String(60, '=') + "\n");
}

// ============================================================================
// КОНЕЦ ФАЙЛА rtos_tasks.cpp
// ============================================================================