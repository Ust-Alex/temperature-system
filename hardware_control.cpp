#include "hardware_control.h"

void initHardware() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n" + String(60, '='));
  Serial.println("    СИСТЕМА МОНИТОРИНГА ТЕМПЕРАТУР - FreeRTOS 3.0");
  Serial.println("    УЛУЧШЕННАЯ ВЕРСИЯ С ПОЛНЫМ СБРОСОМ ДИСПЛЕЯ");
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
  Serial.println(String(60, '=') + "\n");
}

void findSensors() {
  Serial.println("\n🔍 ПОИСК ДАТЧИКОВ (ТОЧНАЯ ПРИВЯЗКА)...");

  for (int i = 0; i < 4; i++) {
    sensors[i].found = false;
    memset(sensors[i].addr, 0, 8);
  }

  int foundCount = 0;

  Serial.println("\n--- Шина A (пин 4, гильза) ---");
  int countA = sensorsA.getDeviceCount();
  Serial.printf("Найдено устройств: %d\n", countA);

  if (countA > 0) {
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

  Serial.println("\n--- Шина B (пин 16, датчики стенки) ---");
  int countB = sensorsB.getDeviceCount();
  Serial.printf("Найдено устройств: %d\n", countB);

  if (countB >= 3) {
    DeviceAddress foundAddrs[3];

    for (int i = 0; i < 3; i++) {
      sensorsB.getAddress(foundAddrs[i], i);
    }

    memcpy(sensors[0].addr, foundAddrs[2], 8);
    sensors[0].found = true;
    sensorsB.setResolution(sensors[0].addr, RESOLUTION);
    foundCount++;
    Serial.print("✅ Датчик 100см (строка 1, верх): ");
    printAddress(sensors[0].addr);
    Serial.println();

    memcpy(sensors[1].addr, foundAddrs[0], 8);
    sensors[1].found = true;
    sensorsB.setResolution(sensors[1].addr, RESOLUTION);
    foundCount++;
    Serial.print("✅ Датчик 75см (строка 2): ");
    printAddress(sensors[1].addr);
    Serial.println();

    memcpy(sensors[2].addr, foundAddrs[1], 8);
    sensors[2].found = true;
    sensorsB.setResolution(sensors[2].addr, RESOLUTION);
    foundCount++;
    Serial.print("✅ Датчик 50см (строка 3): ");
    printAddress(sensors[2].addr);
    Serial.println();

  } else if (countB > 0) {
    Serial.printf("⚠️  Найдено только %d из 3 датчиков стенки\n", countB);

    for (int i = 0; i < min(countB, 3); i++) {
      sensorsB.getAddress(sensors[i].addr, i);
      sensors[i].found = true;
      sensorsB.setResolution(sensors[i].addr, RESOLUTION);
      foundCount++;

      Serial.printf("✅ Датчик стенки назначен строке %d: ", i + 1);
      printAddress(sensors[i].addr);
      Serial.println();
    }
  } else {
    Serial.println("❌ Датчики стенки не найдены на шине B!");
  }

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

  criticalError = !sensors[3].found;
  if (criticalError) {
    Serial.println("\n🚨 КРИТИЧЕСКАЯ ОШИБКА: Гильза не найдена!");
  } else {
    Serial.println("\n✅ Все критически важные датчики найдены");
  }
}

void printAddress(uint8_t* addr) {
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", addr[i]);
  }
}

void initFreeRTOSObjects() {
  Serial.println("Инициализация объектов FreeRTOS...");

  dataQueue = xQueueCreate(5, sizeof(SystemData_t));
  if (dataQueue == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать очередь FreeRTOS!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА ОЧЕРЕДИ!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("   ✅ Очередь данных создана");

  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("❌ ОШИБКА: Не удалось создать мьютекс!");
    tft.fillScreen(COLOR_RED);
    tft.setCursor(20, 100);
    tft.print("ОШИБКА МЬЮТЕКСА!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println("   ✅ Мьютекс создан");
}