/**
 * ============================================================================
 * ФАЙЛ: hardware_control.cpp
 * РЕАЛИЗАЦИЯ ФУНКЦИЙ УПРАВЛЕНИЯ АППАРАТНОЙ ЧАСТЬЮ СИСТЕМЫ
 * 
 * ВЕРСИЯ: 4.4 (ОЧИЩЕННАЯ ОТ МЁРТВОГО КОДА)
 * 
 * ОСОБЕННОСТИ:
 * 1. Инициализация всей аппаратуры (дисплея, датчиков, энкодера)
 * 2. Создание объектов FreeRTOS (очереди, мьютексы)
 * 3. Интеграция с MP3-проигрывателем (инициализация)
 * ============================================================================
 */

#include "hardware_control.h"
#include "encoder_engine.h"   // Модуль для работы с энкодером
#include "mp3_player.h"       // Модуль MP3-проигрывателя
#include "sensors.h"          // Модуль датчиков

// ============================================================================
// ОСНОВНАЯ ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ АППАРАТУРЫ
// ============================================================================

void initHardware() {
  // 1. ИНИЦИАЛИЗАЦИЯ ПОСЛЕДОВАТЕЛЬНОГО ПОРТА ДЛЯ ОТЛАДКИ
  Serial.begin(115200);
  delay(1000); // Задержка для стабилизации Serial ДО запуска FreeRTOS
  
  Serial.println("\n" + String(60, '='));
  Serial.println("    СИСТЕМА МОНИТОРИНГА ТЕМПЕРАТУР - FreeRTOS + ENCODER + MP3");
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
  maxTempWidth = tft.textWidth("00.00") + 10;

  tft.setTextFont(FONT_DELTA);
  maxDeltaWidth = tft.textWidth("-0.00") + 5;

  // Тестовое сообщение на дисплее (3 секунды)
  tft.setTextFont(FONT_DELTA);
  tft.setCursor(5, 100);
  tft.print("esp32ust.local");
  delay(3000);

  // 3. ИНИЦИАЛИЗАЦИЯ ШИН 1-WIRE И ДАТЧИКОВ
  Serial.println("Инициализация шин 1-Wire...");
  sensorsA.begin();
  sensorsB.begin();

  sensors_init();  // Инициализация модуля датчиков

  // 4. ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРА
  Serial.println("Инициализация энкодера...");
  encoder_init();
  
  // 5. СОЗДАНИЕ ОБЪЕКТОВ FREERTOS
  initFreeRTOSObjects();

  // 6. НАЧАЛЬНАЯ ИНИЦИАЛИЗАЦИЯ СИСТЕМНЫХ ДАННЫХ
  sysData.mode = 0;
  sysData.needsRedraw = true;

  for (int i = 0; i < 4; i++) {
    sysData.temps[i] = 0.0f;
    sysData.deltas[i] = 0.0f;
    sysData.colors[i] = 0;
  }

  guildBaseTemp = 0.0f;
  guildColorState = 0;

  // 7. ФИНАЛЬНАЯ НАСТРОЙКА СИСТЕМНЫХ ФЛАГОВ
  criticalError = !sensors[3].found;
  systemInitialized = sensors[3].found;

  forceDisplayRedraw = true;
  lastDisplayMode = 0xFF;

  // 8. ФИНАЛЬНОЕ СООБЩЕНИЕ
  Serial.println("\n✅ Аппаратная часть инициализирована");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println("🎛️  Энкодер готов к работе");
  Serial.println(String(60, '=') + "\n");
}

// Функция findSensors() полностью удалена, так как заменена на sensors_init()