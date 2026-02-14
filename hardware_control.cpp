/** * ФАЙЛ: hardware_control.cpp
 * РЕАЛИЗАЦИЯ ФУНКЦИЙ УПРАВЛЕНИЯ АППАРАТНОЙ ЧАСТЬЮ СИСТЕМЫ
 * 
 * ВЕРСИЯ: 4.3 (С ИСПРАВЛЕННОЙ ЛОГИКОЙ ЗВУКА И ОТЛАДКОЙ)
 * ДАТА: [Текущая дата]
 * 
 * ОСОБЕННОСТИ:
 * 1. Инициализация всей аппаратуры (дисплея, датчиков, энкодера)
 * 2. Поиск и конфигурация датчиков температуры
 * 3. Создание объектов FreeRTOS (очереди, мьютексы)
 * 4. Интеграция с MP3-проигрывателем для звукового оповещения
 * 5. Расширенная отладочная печать для диагностики
 * ============================================================================
 */

#include "hardware_control.h"
#include "encoder_engine.h"   // Модуль для работы с энкодером
#include "mp3_player.h"       // Модуль MP3-проигрывателя
#include "sensors.h" 

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ УПРАВЛЕНИЯ ЗВУКОМ
// ============================================================================

/**
 * СТАТИЧЕСКИЙ ФЛАГ ПРЕДЫДУЩЕГО СОСТОЯНИЯ ОШИБКИ
 * Используется для определения момента перехода criticalError false→true
 * и true→false, чтобы проигрывать звуки только при СМЕНЕ состояния
 */
static bool lastCriticalErrorState = false;

// ============================================================================
// ОСНОВНАЯ ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ АППАРАТУРЫ
// ============================================================================

void initHardware() {
  // 1. ИНИЦИАЛИЗАЦИЯ ПОСЛЕДОВАТЕЛЬНОГО ПОРТА ДЛЯ ОТЛАДКИ
  Serial.begin(115200);
  delay(1000); // Критическая задержка для стабилизации Serial ДО запуска FreeRTOS
  
  Serial.println("\n" + String(60, '='));
  Serial.println("    СИСТЕМА МОНИТОРИНГА ТЕМПЕРАТУР - FreeRTOS 3.0 + ENCODER + MP3");
  Serial.println(String(60, '='));

  // 2. ИНИЦИАЛИЗАЦИЯ TFT ДИСПЛЕЯ
  Serial.println("Инициализация дисплея...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);

  // Настройка метрик шрифтов для оптимизации отображения
  tft.setTextFont(FONT_BIG);
  bigFontHeight = tft.fontHeight();

  tft.setTextFont(FONT_DELTA);
  deltaFontHeight = tft.fontHeight();

  tft.setTextFont(FONT_SMALL);
  smallFontHeight = tft.fontHeight();

  tft.setTextFont(FONT_BIG);
  maxTempWidth = tft.textWidth("00.00");
  maxTempWidth += 10;

  tft.setTextFont(FONT_DELTA);
  maxDeltaWidth = tft.textWidth("-0.00");
  maxDeltaWidth += 5;

  // Тестовое сообщение на дисплее (3 секунды)
  tft.setTextFont(FONT_DELTA);
  tft.setCursor(5, 100);
  tft.print("Save 02.02.2026");
  delay(3000);

  // 3. ИНИЦИАЛИЗАЦИЯ ШИН 1-WIRE И ПОИСК ДАТЧИКОВ
  Serial.println("Инициализация шин 1-Wire...");
  sensorsA.begin();
  sensorsB.begin();

  // findSensors(); // Функция также устанавливает criticalError
  sensors_init();

  // 4. ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРА (новая библиотека EncButton)
  Serial.println("Инициализация энкодера...");
  encoder_init();
  
  // 5. СОЗДАНИЕ ОБЪЕКТОВ FREERTOS (очереди, мьютексы)
  initFreeRTOSObjects();

  // 6. НАЧАЛЬНАЯ ИНИЦИАЛИЗАЦИЯ СИСТЕМНЫХ ДАННЫХ
  sysData.mode = 0;
  sysData.needsRedraw = true;

  for (int i = 0; i < 4; i++) {
    sysData.temps[i] = 0.0f;
    sysData.deltas[i] = 0.0f;
    sysData.colors[i] = 0;

    sensors[i].filterIndex = 0;
    sensors[i].filterSum = 0;
    sensors[i].lostTimer = 0;
    sensors[i].baseTemp = 0.0f;
    for (int j = 0; j < 5; j++) {
      sensors[i].filterBuffer[j] = 0;
    }
  }

  guildBaseTemp = 0.0f;
  guildColorState = 0;

  // 7. ФИНАЛЬНАЯ НАСТРОЙКА СИСТЕМНЫХ ФЛАГОВ
  // criticalError уже установлен в findSensors(), но дублируем для ясности
  criticalError = !sensors[3].found;
  systemInitialized = sensors[3].found;
  
  // Сохраняем начальное состояние для отслеживания изменений
  lastCriticalErrorState = criticalError;

  forceDisplayRedraw = true;
  lastDisplayMode = 0xFF;

  // 8. ФИНАЛЬНОЕ СООБЩЕНИЕ ОБ УСПЕШНОЙ ИНИЦИАЛИЗАЦИИ
  Serial.println("\n✅ Аппаратная часть инициализирована");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println("🎛️  Энкодер готов к работе");
  Serial.println("🔊 MP3-модуль уже проинициализирован");
  Serial.println(String(60, '=') + "\n");
}

// ============================================================================
// ФУНКЦИЯ ПОИСКА И ПРИВЯЗКИ ДАТЧИКОВ ТЕМПЕРАТУРЫ
// ============================================================================

void findSensors() {
  Serial.println("\n🔍 ПОИСК ДАТЧИКОВ (ТОЧНАЯ ПРИВЯЗКА)...");

  // СБРОС ВСЕХ ФЛАГОВ ПЕРЕД НОВЫМ ПОИСКОМ
  for (int i = 0; i < 4; i++) {
    sensors[i].found = false;
    memset(sensors[i].addr, 0, 8);
  }

  int foundCount = 0;

  // ПОИСК НА ШИНЕ A (ДАЧТИК ГИЛЬЗЫ, GPIO4)
  Serial.println("\n--- Шина A (пин 4, гильза) ---");
  int countA = sensorsA.getDeviceCount();
  Serial.printf("Найдено устройств: %d\n", countA);

  if (countA > 0) {
    // На шине A должен быть только датчик гильзы (индекс 0)
    sensorsA.getAddress(sensors[3].addr, 0);
    sensors[3].found = true;
    sensorsA.setResolution(sensors[3].addr, RESOLUTION);
    foundCount++;

    Serial.print("✅ Гильза 25см (строка 4) назначена: ");
    printAddress(sensors[3].addr);
    Serial.println();
  } else {
    Serial.println("❌ Гильза не найдена на шине A!");
  }

  // ПОИСК НА ШИНЕ B (ДАТЧИКИ СТЕНОК, GPIO16)
  Serial.println("\n--- Шина B (пин 16, датчики стенки) ---");
  int countB = sensorsB.getDeviceCount();
  Serial.printf("Найдено устройств: %d\n", countB);

  // ИДЕАЛЬНЫЙ СЛУЧАЙ: НАЙДЕНЫ ВСЕ 3 ДАТЧИКА СТЕНОК
  if (countB >= 3) {
    DeviceAddress foundAddrs[3];

    // Считываем все адреса с шине B
    for (int i = 0; i < 3; i++) {
      sensorsB.getAddress(foundAddrs[i], i);
    }

    // ПРИВЯЗКА: Адрес с индексом 2 → датчик 100см (верхний)
    memcpy(sensors[0].addr, foundAddrs[2], 8);
    sensors[0].found = true;
    sensorsB.setResolution(sensors[0].addr, RESOLUTION);
    foundCount++;
    Serial.print("✅ Датчик 100см (строка 1, верх): ");
    printAddress(sensors[0].addr);
    Serial.println();

    // Адрес с индексом 0 → датчик 75см (средний)
    memcpy(sensors[1].addr, foundAddrs[0], 8);
    sensors[1].found = true;
    sensorsB.setResolution(sensors[1].addr, RESOLUTION);
    foundCount++;
    Serial.print("✅ Датчик 75см (строка 2): ");
    printAddress(sensors[1].addr);
    Serial.println();

    // Адрес с индексом 1 → датчик 50см (нижний)
    memcpy(sensors[2].addr, foundAddrs[1], 8);
    sensors[2].found = true;
    sensorsB.setResolution(sensors[2].addr, RESOLUTION);
    foundCount++;
    Serial.print("✅ Датчик 50см (строка 3): ");
    printAddress(sensors[2].addr);
    Serial.println();
  } 
  // ЧАСТИЧНЫЙ СЛУЧАЙ: НАЙДЕНЫ НЕ ВСЕ ДАТЧИКИ
  else if (countB > 0) {
    Serial.printf("⚠️  Найдено только %d из 3 датчиков стенки\n", countB);

    // Привязываем найденные датчики по порядку
    for (int i = 0; i < min(countB, 3); i++) {
      sensorsB.getAddress(sensors[i].addr, i);
      sensors[i].found = true;
      sensorsB.setResolution(sensors[i].addr, RESOLUTION);
      foundCount++;

      Serial.printf("✅ Датчик стенки назначен строке %d: ", i + 1);
      printAddress(sensors[i].addr);
      Serial.println();
    }
  } 
  // ДАТЧИКИ НЕ НАЙДЕНЫ
  else {
    Serial.println("❌ Датчики стенки не найдены на шине B!");
  }

  // СВОДНАЯ ИНФОРМАЦИЯ О РЕЗУЛЬТАТАХ ПОИСКА
  Serial.printf("\n📊 ИТОГО: %d из 4 датчиков найдено\n", foundCount);

  Serial.println("\n📋 ТАБЛИЦА СООТВЕТСТВИЯ:");
  for (int i = 0; i < 4; i++) {
    Serial.printf("  [%d] %s: ", i, sensorNames[i]);
    if (sensors[i].found) {
      Serial.print("✅ ");
      printAddress(sensors[i].addr);
    } else {
      Serial.print("❌ Не найден");
    }
    Serial.println();
  }

  // ==========================================================================
  // ДОБАВЛЕНО: ЛОГИКА ЗВУКОВОГО ОПОВЕЩЕНИЯ С ОТЛАДОЧНОЙ ПЕЧАТЬЮ
  // ==========================================================================
  
  // Сохраняем предыдущее состояние ДЛЯ СРАВНЕНИЯ (для восстановления)
  bool previousErrorState = criticalError;
  
  // Устанавливаем новое состояние на основе наличия датчика гильзы
  criticalError = !sensors[3].found;
  
  // ОТЛАДОЧНАЯ ПЕЧАТЬ ДЛЯ ДИАГНОСТИКИ
  Serial.printf("\n[SOUND DEBUG] Предыдущее состояние: %s, Новое состояние: %s\n",
               previousErrorState ? "ОШИБКА" : "НОРМА",
               criticalError ? "ОШИБКА" : "НОРМА");
  Serial.printf("[SOUND DEBUG] mp3PlayerReady = %d\n", mp3PlayerReady);
  
  // Обновляем глобальную переменную для отслеживания
  lastCriticalErrorState = criticalError;
  
  // ОБРАБОТКА СИТУАЦИЙ СО ЗВУКОМ
  if (criticalError) {
    Serial.println("\n🚨 КРИТИЧЕСКАЯ ОШИБКА: Гильза не найдена!");
    
    // ЗВУКОВОЕ ОПОВЕЩЕНИЕ ПРИ ОБНАРУЖЕНИИ ОШИБКИ
    // Играем ВСЕГДА при обнаружении ошибки, не только при переходе
    if (mp3PlayerReady) {
        Serial.println("   🔊 Обнаружена критическая ошибка");
        
        // 1. ПРЕРЫВАНИЕ ЛЮБОГО ТЕКУЩЕГО ВОСПРОИЗВЕДЕНИЯ
        Mp3Command_t cmdStop = {MP3_CMD_STOP, 0};
        if (sendMP3Command(cmdStop)) {
            Serial.println("   ⏹️  Текущее воспроизведение остановлено");
        } else {
            Serial.println("   ❌ Не удалось отправить команду STOP");
        }
        
        // 2. КОРОТКАЯ ПАУЗА ДЛЯ СТАБИЛЬНОСТИ СВЯЗИ
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 3. ЗАПУСК КРИТИЧЕСКОГО СИГНАЛА (0002.mp3)
        Mp3Command_t cmdAlert = {MP3_CMD_PLAY_TRACK, 2}; // 0002.mp3
        if (sendMP3Command(cmdAlert)) {
            Serial.println("   🔉 Запущен критический сигнал (0002.mp3)");
        } else {
            Serial.println("   ❌ Не удалось отправить команду PLAY 0002.mp3");
        }
        
        // 4. КОМАНДА НА ВКЛЮЧЕНИЕ РЕЖИМА ПОВТОРА (каждые 10 секунд)
        Mp3Command_t cmdRepeat = {MP3_CMD_ENABLE_REPEAT, 10}; // Повторять каждые 10 секунд
        if (sendMP3Command(cmdRepeat)) {
            Serial.println("   🔁 Режим повтора включен (интервал: 10 сек)");
        } else {
            Serial.println("   ❌ Не удалось отправить команду повтора");
        }
        
    } else {
        Serial.println("   🔇 Звук отключен: MP3-проигрыватель не готов");
        Serial.println("   [DEBUG] Проверьте initMP3Player() и подключение DFPlayer");
    }
  } else {
    // ДАТЧИК ГИЛЬЗЫ НАЙДЕН (ОШИБКИ НЕТ)
    Serial.println("\n✅ Все критически важные датчики найдены");
    
    // ЗВУК ВОССТАНОВЛЕНИЯ ТОЛЬКО ПРИ ПЕРЕХОДЕ ИЗ СОСТОЯНИЯ ОШИБКИ
    // Проверяем, БЫЛА ЛИ ошибка до этого (по предыдущему состоянию)
    if (previousErrorState && mp3PlayerReady) {
        Serial.println("   🔊 Обнаружено восстановление датчика гильзы");
        
        // 1. КОМАНДА НА ВЫКЛЮЧЕНИЕ РЕЖИМА ПОВТОРА
        Mp3Command_t cmdStopRepeat = {MP3_CMD_DISABLE_REPEAT, 0};
        if (sendMP3Command(cmdStopRepeat)) {
            Serial.println("   ⏹️  Режим повтора выключен");
        }
        
        // 2. КОРОТКАЯ ПАУЗА
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 3. ОДИНОЧНЫЙ СИГНАЛ ВОССТАНОВЛЕНИЯ
        Mp3Command_t cmdRecover = {MP3_CMD_PLAY_TRACK, 2}; // Тот же 0002.mp3
        if (sendMP3Command(cmdRecover)) {
            Serial.println("   🔉 Проигран сигнал восстановления (0002.mp3)");
        }
    } else if (previousErrorState && !mp3PlayerReady) {
        Serial.println("   [DEBUG] Восстановление обнаружено, но MP3 не готов");
    }
  }
} // Конец функции findSensors()

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: ВЫВОД АДРЕСА ДАТЧИКА В SERIAL
// ============================================================================

void printAddress(uint8_t* addr) {
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", addr[i]);
  }
}

// ============================================================================
// ФУНКЦИЯ СОЗДАНИЯ ОБЪЕКТОВ FREERTOS
// ============================================================================

void initFreeRTOSObjects() {
  Serial.println("Инициализация объектов FreeRTOS...");

  // 1. СОЗДАНИЕ ОЧЕРЕДИ ДАННЫХ (передача данных между задачами)
  dataQueue = xQueueCreate(5, sizeof(SystemData_t));
  if (dataQueue == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать очередь FreeRTOS!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА ОЧЕРЕДИ!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("   ✅ Очередь данных создана");

  // 2. СОЗДАНИЕ МЬЮТЕКСА ДЛЯ ЗАЩИТЫ ОБЩИХ ДАННЫХ
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать мьютекс!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА МЬЮТЕКСА!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); 
  }
  Serial.println("   ✅ Мьютекс создан");
  
  // 3. СОЗДАНИЕ ОЧЕРЕДИ СОБЫТИЙ ДЛЯ ЭНКОДЕРА
  eventQueue = xQueueCreate(10, sizeof(EncoderEvent_t));
  if (eventQueue == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать очередь событий энкодера!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 120);
    tft.print("ОШИБКА ОЧЕРЕДИ СОБЫТИЙ!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("   ✅ Очередь событий создана (10 событий)");
}