/**
 * temperature_system.ino
 * Главный файл системы контроля температуры с RTOS
 * Версия с интеграцией Wi-Fi задачи в общий поток данных
 */

#include "system_config.h"
#include "rtos_tasks.h"
#include "wifi_ap_module.h"  // Wi-Fi модуль теперь управляется своей задачей

/* ------------------------------------------------------------
   Глобальные объекты RTOS (определяются здесь, объявлены в system_config.h)
   Эти объекты создаются в setup() и используются всеми задачами.
------------------------------------------------------------ */
QueueHandle_t dataQueue = NULL;      // Очередь для передачи данных между задачами
SemaphoreHandle_t dataMutex = NULL;  // Мьютекс для защиты общего доступа к sysData






/* ------------------------------------------------------------
   Глобальные переменные состояния системы (определения)
   Объявлены как 'extern' в system_config.h
------------------------------------------------------------ */
TFT_eSPI tft;
OneWire oneWireA(ONE_WIRE_BUS_A);
OneWire oneWireB(ONE_WIRE_BUS_B);
DallasTemperature sensorsA(&oneWireA);
DallasTemperature sensorsB(&oneWireB);

SystemData_t sysData = { 0 };  // Основная структура данных системы
Sensor_t sensors[4] = { 0 };   // Массив структур состояния датчиков

// Флаги и переменные состояния
bool baseSaved = false;
bool systemInitialized = false;
bool criticalError = false;
bool forceDisplayRedraw = false;
uint8_t lastDisplayMode = 0;
uint16_t lastGlobalBgColor = COLOR_BLACK;
float timeRefTemp = 0.0f;
uint32_t timeStartMs = 0;
bool timeIsCounting = false;
float guildBaseTemp = 0.0f;
uint8_t guildColorState = 0;
float lastDisplayTemps[4] = { 0 };
float lastDisplayDeltas[4] = { 0 };

// Константы для отображения
const char* sensorNames[4] = { "100см", "75см", "50см", "Гильза" };
int bigFontHeight, deltaFontHeight, smallFontHeight;
int maxTempWidth, maxDeltaWidth;
const int displayYPositions[4] = { 10, 80, 150, 220 };  // Y-координаты для каждой строки на дисплее

/* ------------------------------------------------------------
   ФУНКЦИЯ НАСТРОЙКИ (выполняется один раз при старте)
   Изменения по новому плану:
   1. Создание объектов RTOS (очередь, мьютекс)
   2. Инициализация оборудования (без Wi-Fi!)
   3. Создание ВСЕХ задач RTOS (включая Wi-Fi)
   4. Wi-Fi инициализируется внутри своей задачи, а не здесь.
------------------------------------------------------------ */
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n" + String(60, '='));
  // ... заголовок

  // 1. Создание очереди и мьютекса
  dataMutex = xSemaphoreCreateMutex();
  dataQueue = xQueueCreate(5, sizeof(SystemData_t));
  Serial.println("[OK] Очередь и мьютекс созданы.");

  // 2. Инициализация оборудования (ОДИН раз)
  Serial.println("[DEBUG] Вызываем initHardware()");
  // initHardware();
  // Serial.println("[DEBUG] initHardware() завершена");
  // Тестовый код вместо initHardware()
  Serial.println("[DEBUG] initHardware отключена, ждём 5 секунд...");
  for (int i = 5; i > 0; i--) {
    Serial.printf("[DEBUG] %d...\n", i);
    delay(1000);
  }




  // 3. СОЗДАНИЕ ЗАДАЧ (ЗАКОММЕНТИРОВАТЬ ДЛЯ ТЕСТА)
  // Serial.println("[DEBUG] Создаем задачи...");
  create_rtos_tasks();

  Serial.println("[DEBUG] setup() завершен, переходим в loop()");
  Serial.println(String(60, '=') + "\n");
}


/* ------------------------------------------------------------
   ГЛАВНЫЙ ЦИКЛ (loop)
   В архитектуре с RTOS эта функция становится задачей с самым низким приоритетом (idle).
   Её можно использовать для фоновой диагностики или низкоприоритетной логики.
------------------------------------------------------------ */
void loop() {
  // Пример: Раз в 60 секунд выводим сводку о состоянии системы.
  static TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xDiagnosticPeriod = 60000 / portTICK_PERIOD_MS;  // 60 секунд

  vTaskDelayUntil(&xLastWakeTime, xDiagnosticPeriod);  // Точная периодическая задержка

  // Диагностический вывод (можно отключить в рабочей версии)
  UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("[ДИАГН.] Стек loop: %u слов | ", uxHighWaterMark);

  if (dataQueue != NULL) {
    Serial.printf("Очередь: %d/%d | ", uxQueueMessagesWaiting(dataQueue), uxQueueSpacesAvailable(dataQueue));
  }
  Serial.printf("Свободная память: %d байт\n", ESP.getFreeHeap());
}