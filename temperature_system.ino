/**
 * ============================================================================
 * ФАЙЛ: temperature_system.ino
 * ГЛАВНЫЙ ФАЙЛ ПРОЕКТА - ТОЧКА ВХОДА СИСТЕМЫ КОНТРОЛЯ ТЕМПЕРАТУРЫ
 * 
 * ВЕРСИЯ: 5.1 (С ИСПРАВЛЕННЫМ ПОРЯДКОМ ИНИЦИАЛИЗАЦИИ MP3)
 * 
 * ОСОБЕННОСТИ:
 * 1. Точка входа Arduino (setup() и loop())
 * 2. Объявления всех глобальных переменных и объектов
 * 3. Инициализация MP3 ПЕРЕД аппаратурой (чтобы звук ошибки работал)
 * 4. Фоновый мониторинг состояния системы
 * 5. Поддержка MP3-проигрывателя для звукового сопровождения
 * ============================================================================
 */

// Основные заголовочные файлы системы
#include "system_config.h"  // Конфигурация пинов и параметров системы
#include "rtos_tasks.h"     // Задачи FreeRTOS и их создание
#include "calibration_simple.h"
#include "mp3_player.h"  // Модуль MP3-проигрывателя

// ============================================================================
// РАЗДЕЛ: ГЛОБАЛЬНЫЕ ОБЪЕКТЫ И ПЕРЕМЕННЫЕ СИСТЕМЫ
// ============================================================================

// ОБЪЕКТЫ ДЛЯ РАБОТЫ С ДИСПЛЕЕМ И ДАТЧИКАМИ
// ------------------------------------------------------------
TFT_eSPI tft;

OneWire oneWireA(ONE_WIRE_BUS_A);
OneWire oneWireB(ONE_WIRE_BUS_B);

DallasTemperature sensorsA(&oneWireA);
DallasTemperature sensorsB(&oneWireB);

// ОБЪЕКТЫ ДЛЯ РАБОТЫ С MP3-ПРОИГРЫВАТЕЛЕМ
// ------------------------------------------------------------
HardwareSerial dfplayerSerial(2);      // Используем аппаратный UART2 (Serial2)
DFRobotDFPlayerMini myDFPlayer;        // Объект DFPlayer Mini
QueueHandle_t mp3CommandQueue = NULL;  // Очередь команд для задачи MP3
bool mp3PlayerReady = false;           // Флаг готовности плеера

// ОСНОВНЫЕ СТРУКТУРЫ ДАННЫХ СИСТЕМЫ
// ------------------------------------------------------------
SystemData_t sysData;                // Основные данные системы
Sensor_t sensors[4];                 // Данные датчиков температуры
QueueHandle_t dataQueue = NULL;      // Очередь для передачи данных дисплею
SemaphoreHandle_t dataMutex = NULL;  // Мьютекс для защиты доступа к sysData

// ФЛАГИ СОСТОЯНИЯ СИСТЕМЫ
bool baseSaved = false;           // Флаг сохранения базовой температуры
bool systemInitialized = false;   // Флаг полной инициализации системы
bool criticalError = false;       // Флаг критической ошибки (потеря гильзы)
bool forceDisplayRedraw = false;  // Флаг принудительной перерисовки дисплея

// СОСТОЯНИЕ ИНТЕРФЕЙСА И ОТОБРАЖЕНИЯ
uint8_t lastDisplayMode = 0xFF;       // Предыдущий режим отображения
uint16_t lastGlobalBgColor = 0xFFFF;  // Предыдущий цвет фона
float timeRefTemp = 0.0f;             // Опорная температура для таймера стабилизации
uint32_t timeStartMs = 0;             // Время начала отсчёта стабилизации
bool timeIsCounting = false;          // Флаг активного отсчёта времени

// РЕЖИМ MODE2 (РАБОЧИЙ РЕЖИМ)
float guildBaseTemp = 0.0f;   // Базовая температура гильзы для MODE2
uint8_t guildColorState = 0;  // Цветовое состояние: 0=зелёный,1=жёлтый,2=красный

// КЭШ ОТОБРАЖЕНИЯ
float lastDisplayTemps[4] = { 0 };   // Последние отображённые температуры
float lastDisplayDeltas[4] = { 0 };  // Последние отображённые дельты

// КОНСТАНТЫ И МЕТРИКИ
const char* sensorNames[4] = {  // Названия датчиков для отображения
  "СТЕНКА 100см",
  "СТЕНКА 75см",
  "СТЕНКА 50cm",
  "ГИЛЬЗА"
};

// МЕТРИКИ ШРИФТОВ
int bigFontHeight = 0;    // Высота большого шрифта
int deltaFontHeight = 0;  // Высота шрифта дельты
int smallFontHeight = 0;  // Высота малого шрифта
int maxTempWidth = 0;     // Максимальная ширина строки температуры
int maxDeltaWidth = 0;    // Максимальная ширина строки дельты

// ПОЗИЦИИ ОТОБРАЖЕНИЯ
const int displayYPositions[4] = {  // Y-координаты для данных датчиков
  0, 60, 120, 180
};

// ЭНКОДЕР И УПРАВЛЕНИЕ ИНТЕРФЕЙСОМ
QueueHandle_t eventQueue = NULL;  // Очередь событий энкодера
uint8_t systemState = 0;          // Текущее состояние интерфейса (0=главный, 1=меню)

// ============================================================================
// ФУНКЦИЯ SETUP(): ОДНОКРАТНАЯ ИНИЦИАЛИЗАЦИЯ ПРИ ЗАПУСКЕ СИСТЕМЫ
// ============================================================================

void setup() {
  // НАЧАЛО ИНИЦИАЛИЗАЦИИ - КРИТИЧЕСКИ ВАЖНЫЙ ЭТАП
  // ------------------------------------------------------------

  // ШАГ 1: ИНИЦИАЛИЗАЦИЯ ПОСЛЕДОВАТЕЛЬНОГО ПОРТА (Serial)
  Serial.begin(115200);
  delay(2000);  // Критическая задержка для стабилизации ESP32 и подключения Serial монитора

  Serial.println("\n" + String(70, '='));
  Serial.println("🚀 ЗАПУСК СИСТЕМЫ КОНТРОЛЯ ТЕМПЕРАТУРЫ (ВЕРСИЯ 5.1)");
  Serial.println("🎵 С ПОДДЕРЖКОЙ MP3-ПРОИГРЫВАТЕЛЯ (исправленный порядок)");
  Serial.println(String(70, '='));

  // ШАГ 2: АППАРАТНАЯ ДИАГНОСТИКА ПЕРЕД ИНИЦИАЛИЗАЦИЕЙ
  Serial.println("\n🔍 ПРЕДВАРИТЕЛЬНАЯ ДИАГНОСТИКА:");

  // 2.1 Проверка пинов датчиков температуры
  Serial.printf("  Датчик гильзы: GPIO%d (ONE_WIRE_BUS_A)\n", ONE_WIRE_BUS_A);
  Serial.printf("  Датчики стенок: GPIO%d (ONE_WIRE_BUS_B)\n", ONE_WIRE_BUS_B);

  // 2.2 Проверка напряжения на пинах
  pinMode(ONE_WIRE_BUS_A, INPUT_PULLUP);
  pinMode(ONE_WIRE_BUS_B, INPUT_PULLUP);
  delay(50);

  int voltageA = digitalRead(ONE_WIRE_BUS_A);
  int voltageB = digitalRead(ONE_WIRE_BUS_B);

  Serial.printf("  Напряжение GPIO%d: %s\n", ONE_WIRE_BUS_A,
                voltageA == HIGH ? "HIGH ✅" : "LOW ⚠️ (возможная проблема!)");
  Serial.printf("  Напряжение GPIO%d: %s\n", ONE_WIRE_BUS_B,
                voltageB == HIGH ? "HIGH ✅" : "LOW ⚠️ (возможная проблема!)");

  // 2.3 Критическая проверка: если пины прижаты к GND - есть проблема с подключением
  if (voltageA == LOW || voltageB == LOW) {
    Serial.println("\n❌ КРИТИЧЕСКАЯ ОШИБКА АППАРАТУРЫ!");
    Serial.println("   Пины датчиков показывают LOW (прижаты к GND)");
    Serial.println("   СИСТЕМА НЕ БУДЕТ ЗАПУЩЕНА ДО ИСПРАВЛЕНИЯ!");
    Serial.println(String(70, '='));

    while (true) {
      delay(1000);
      Serial.print(".");
    }
  }

  // ШАГ 3: ИНИЦИАЛИЗАЦИЯ MP3-ПРОИГРЫВАТЕЛЯ (ПЕРВЫМ!)
  // Должна выполняться ДО initHardware(), чтобы звук ошибки работал
  Serial.println("\n[MP3] Инициализация звукового модуля...");
  if (initMP3Player()) {
    Serial.println("🎵 MP3-проигрыватель инициализирован успешно");
  } else {
    Serial.println("⚠️  MP3-проигрыватель не обнаружен (работаем без звука)");
  }

  // ШАГ 4: ОСНОВНАЯ ИНИЦИАЛИЗАЦИЯ АППАРАТУРЫ
  Serial.println("\n✅ MP3-модуль готов");
  Serial.println("🔄 Запуск основной инициализации аппаратуры...");

  initHardware();  // Инициализация дисплея, датчиков, энкодера

  loadOffsetsFromEEPROM();

  // ШАГ 5: ДОПОЛНИТЕЛЬНАЯ ПРОВЕРКА ПОСЛЕ ИНИЦИАЛИЗАЦИИ
  Serial.println("\n[INIT] Проверка состояния системы после инициализации:");

  if (!sensors[3].found) {
    Serial.println("⚠️  ВНИМАНИЕ: Датчик гильзы не обнаружен после инициализации");
    Serial.println("   Система попытается найти его автоматически позже");
  }

  // ШАГ 6: СОЗДАНИЕ ЗАДАЧ FREERTOS
  Serial.println("\n[INIT] Создание задач FreeRTOS...");
  create_rtos_tasks();

  // СОЗДАНИЕ ЗАДАЧИ MP3-ПРОИГРЫВАТЕЛЯ
  // Создаём только если плеер успешно инициализирован
  if (mp3PlayerReady && mp3CommandQueue != NULL) {
    xTaskCreate(
      taskMP3,       // Функция задачи (из mp3_player.cpp)
      "MP3 Player",  // Имя задачи для отладки
      4096,          // Размер стека (байт) - достаточно для DFPlayer
      NULL,          // Параметры (не нужны)
      1,             // ПРИОРИТЕТ: 1 (низкий - звук не критичен)
      NULL           // Дескриптор задачи (не сохраняем)
    );
    Serial.println("🎵 Задача MP3 создана (приоритет: 1)");

    // КОРОТКАЯ ПАУЗА, чтобы команды из findSensors() обработались первыми
    vTaskDelay(pdMS_TO_TICKS(500));

    // ОТПРАВКА КОМАНДЫ НА СТАРТОВЫЙ ЗВУК (трек 1)
    Mp3Command_t startSound;
    startSound.cmd = MP3_CMD_PLAY_TRACK;
    startSound.param = 1;  // Трек 0001.mp3

    if (sendMP3Command(startSound)) {
      Serial.println("🎵 Команда на воспроизведение трека #1 отправлена");
    } else {
      Serial.println("⚠️  Не удалось отправить команду стартового звука");
    }

    // ТЕСТОВАЯ КОМАНДА: проверим работу очереди через 3 секунды
    // Можно удалить после тестирования
    vTaskDelay(pdMS_TO_TICKS(3000));
    Serial.println("\n[TEST] Отправка тестовой команды MP3...");
    Mp3Command_t testCmd = { MP3_CMD_PLAY_TRACK, 2 };  // 0002.mp3
    if (sendMP3Command(testCmd)) {
      Serial.println("[TEST] Тестовая команда отправлена (трек 2)");
    }

  } else {
    Serial.println("⚠️  Задача MP3 не создана (плеер не готов или очередь не создана)");
    Serial.printf("   mp3PlayerReady=%d, mp3CommandQueue=%p\n",
                  mp3PlayerReady, mp3CommandQueue);
  }

  // ШАГ 7: ФИНАЛЬНОЕ СООБЩЕНИЕ О ЗАПУСКЕ
  Serial.println("\n" + String(70, '='));
  Serial.println("✅ СИСТЕМА УСПЕШНО ЗАПУЩЕНА");
  Serial.println("📊 Основные характеристики:");
  Serial.printf("   - Режим: %s\n", sysData.mode == 0 ? "СТАБИЛИЗАЦИЯ (MODE1)" : "РАБОЧИЙ (MODE2)");
  Serial.printf("   - Датчик гильзы: %s\n", sensors[3].found ? "ОБНАРУЖЕН ✅" : "НЕ НАЙДЕН ⚠️");
  Serial.printf("   - MP3-проигрыватель: %s\n", mp3PlayerReady ? "ГОТОВ ✅" : "НЕДОСТУПЕН ⚠️");
  Serial.printf("   - Инициализирована: %s\n", systemInitialized ? "ДА ✅" : "НЕТ ⚠️");
  Serial.printf("   - Критическая ошибка: %s\n", criticalError ? "ДА ❌" : "НЕТ ✅");
  Serial.println(String(70, '='));
  Serial.println("\n🎛️  Команды управления (введите в Serial монитор):");
  Serial.println("   FIND    - Принудительный поиск датчиков");
  Serial.println("   STATUS  - Подробный статус системы");
  Serial.println("   MODE1   - Режим стабилизации");
  Serial.println("   MODE2   - Рабочий режим");
  Serial.println("   HELP    - Полный список команд");
  Serial.println(String(70, '=') + "\n");
}

// ============================================================================
// ФУНКЦИЯ LOOP(): ОСНОВНОЙ ЦИКЛ ARDUINO (ФОНОВЫЙ МОНИТОРИНГ)
// ============================================================================

void loop() {
  static uint32_t lastSystemCheck = 0;
  static uint32_t lastSensorCheck = 0;
  static uint32_t lastMP3Check = 0;  // НОВОЕ: для мониторинга состояния MP3
  uint32_t currentMillis = millis();

  // ПРОВЕРКА 1: ПЕРИОДИЧЕСКИЙ СТАТУС СИСТЕМЫ (каждые 5 минут)
  if (currentMillis - lastSystemCheck > 300000) {
    Serial.println("\n[SYSTEM CHECK] " + String(55, '='));
    Serial.printf("Время работы: %lu минут %lu секунд\n",
                  currentMillis / 60000, (currentMillis % 60000) / 1000);
    Serial.printf("Текущий режим: %d (%s)\n",
                  sysData.mode,
                  sysData.mode == 0 ? "СТАБИЛИЗАЦИЯ" : "РАБОЧИЙ");
    Serial.printf("Датчик гильзы: %s\n",
                  sensors[3].found ? "НАЙДЕН ✅" : "ПОТЕРЯН ❌");
    Serial.printf("MP3-проигрыватель: %s\n",  // НОВОЕ
                  mp3PlayerReady ? "ГОТОВ ✅" : "ОТКЛЮЧЕН ⚠️");
    Serial.printf("Система инициализирована: %s\n",
                  systemInitialized ? "ДА ✅" : "НЕТ ⚠️");
    Serial.printf("Критическая ошибка: %s\n",
                  criticalError ? "ДА ❌ (гильза!)" : "НЕТ ✅");
    Serial.printf("Очередь данных: %s\n",
                  dataQueue ? "СОЗДАНА ✅" : "ОТСУТСТВУЕТ ❌");
    Serial.printf("Очередь MP3: %s\n",  // НОВОЕ
                  mp3CommandQueue ? "СОЗДАНА ✅" : "ОТСУТСТВУЕТ ❌");
    Serial.println(String(55, '=') + "\n");

    lastSystemCheck = currentMillis;
  }

  // ПРОВЕРКА 2: АВТОМАТИЧЕСКИЙ ПОИСК ДАТЧИКОВ ПРИ ДЛИТЕЛЬНОЙ ПОТЕРЕ
  if (currentMillis - lastSensorCheck > 60000) {
    bool anySensorFound = false;
    for (int i = 0; i < 4; i++) {
      if (sensors[i].found) {
        anySensorFound = true;
        break;
      }
    }

    if (!anySensorFound) {
      Serial.println("\n⚠️  [AUTO-RECONNECT] Все датчики потеряны более 1 минуты");
      Serial.println("   Попытка автоматического восстановления...");

      attemptReconnect();

      Serial.println("   [AUTO-RECONNECT] Попытка завершена");
    }

    lastSensorCheck = currentMillis;
  }

  // ПРОВерка 3: МОНИТОРИНГ СОСТОЯНИЯ MP3 (каждые 2 минуты) - НОВОЕ
  if (currentMillis - lastMP3Check > 120000) {
    if (mp3PlayerReady) {
      bool isPlaying = isMP3Playing();
      Serial.printf("[MP3 STATUS] Воспроизведение: %s, Очередь: %d/%d\n",
                    isPlaying ? "ИДЁТ" : "ОСТАНОВЛЕНО",
                    uxQueueMessagesWaiting(mp3CommandQueue),
                    10 - uxQueueSpacesAvailable(mp3CommandQueue));
    }
    lastMP3Check = currentMillis;
  }

  // КОРОТКАЯ ПАУЗА ДЛЯ ДРУГИХ ЗАДАЧ FREERTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}