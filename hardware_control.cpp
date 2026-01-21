#include "hardware_control.h"
#include "encoder_engine.h" // ДОБАВЛЕНО: новый модуль энкодера

void initHardware() {
  Serial.begin(115200);
  delay(1000); // Оставляем! Задержка для стабилизации Serial ДО запуска FreeRTOS
  
  Serial.println("\n" + String(60, '='));
  Serial.println("    СИСТЕМА МОНИТОРИНГА ТЕМПЕРАТУР - FreeRTOS 3.0 + ENCODER");
  Serial.println(String(60, '='));

  Serial.println("Инициализация дисплея...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);

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

  tft.setTextFont(FONT_BIG);
  tft.setCursor(20, 100);
  tft.print("Загрузка системы...");

  Serial.println("Инициализация шин 1-Wire...");
  sensorsA.begin();
  sensorsB.begin();

  findSensors();
  
  // ДОБАВЛЕНО: Инициализация энкодера (перед созданием объектов FreeRTOS)
  encoder_init();
  
  initFreeRTOSObjects();

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

  criticalError = !sensors[3].found;
  systemInitialized = sensors[3].found;

  forceDisplayRedraw = true;
  lastDisplayMode = 0xFF;

  Serial.println("\n✅ Аппаратная часть инициализирована");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println("🎛️  Энкодер готов к работе");
  Serial.println(String(60, '=') + "\n");
}

void findSensors() {
  // ... (эта функция без изменений, оставляем как было) ...
}

void printAddress(uint8_t* addr) {
  // ... (эта функция без изменений, оставляем как было) ...
}

void initFreeRTOSObjects() {
  Serial.println("Инициализация объектов FreeRTOS...");

  // 1. СОЗДАНИЕ ОЧЕРЕДИ ДАННЫХ (было)
  dataQueue = xQueueCreate(5, sizeof(SystemData_t));
  if (dataQueue == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать очередь FreeRTOS!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА ОЧЕРЕДИ!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // ИСПРАВЛЕНО: замена delay
  }
  Serial.println("   ✅ Очередь данных создана");

  // 2. СОЗДАНИЕ МЬЮТЕКСА (было)
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать мьютекс!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА МЬЮТЕКСА!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // ИСПРАВЛЕНО: замена delay
  }
  Serial.println("   ✅ Мьютекс создан");
  
  // 3. ДОБАВЛЕНО: СОЗДАНИЕ ОЧЕРЕДИ СОБЫТИЙ ДЛЯ ЭНКОДЕРА
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