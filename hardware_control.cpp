/**
 * ============================================================================
 * ФАЙЛ: hardware_control.cpp
 * РЕАЛИЗАЦИЯ ФУНКЦИЙ УПРАВЛЕНИЯ АППАРАТНОЙ ЧАСТЬЮ СИСТЕМЫ
 * ВЕРСИЯ: 6.0 (ДЛЯ 6 ДАТЧИКОВ, 4 ШИН)
 * ============================================================================
 */

#include "hardware_control.h"
#include "encoder_engine.h"
#include "mp3_player.h"
#include "sensors.h"

// ============================================================================
// ОСНОВНАЯ ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ АППАРАТУРЫ
// ============================================================================

void initHardware() {
  // 1. ИНИЦИАЛИЗАЦИЯ ПОСЛЕДОВАТЕЛЬНОГО ПОРТА
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n" + String(60, '='));
  Serial.println("    СИСТЕМА МОНИТОРИНГА ТЕМПЕРАТУР - 6 ДАТЧИКОВ, 4 ШИНЫ");
  Serial.println(String(60, '='));

  // 2. ИНИЦИАЛИЗАЦИЯ TFT ДИСПЛЕЯ
  Serial.println("Инициализация дисплея...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);

  // Настройка метрик шрифтов
  tft.setTextFont(FONT_BIG);
  bigFontHeight = tft.fontHeight();

  tft.setTextFont(FONT_DELTA);
  deltaFontHeight = tft.fontHeight();

  tft.setTextFont(FONT_SMALL);
  smallFontHeight = tft.fontHeight();

  tft.setTextFont(FONT_BIG);
  maxTempWidth = tft.textWidth("00.00") + 10;

  // ========================================================================
  // ПРЕДРАССЧИТЫВАЕМ РАЗМЕРЫ ДЛЯ ОЧИСТКИ ОБЛАСТИ ТЕМПЕРАТУРЫ
  // ========================================================================
  tft.setTextFont(FONT_BIG);
  bigFontHeightClear = tft.fontHeight();
  bigTempWidthClear = tft.textWidth("00.00") + 10;

  tft.setTextFont(FONT_DELTA);
  deltaFontHeightClear = tft.fontHeight();
  deltaTempWidthClear = tft.textWidth("00.00") + 10;

  tft.setTextFont(FONT_BIG);

  // Тестовое сообщение
  tft.setTextFont(FONT_DELTA);
  tft.setCursor(5, 100);
  tft.print("esp32ust.local");
  tft.setCursor(5, 150);
  tft.print("192.168.1.11");
  delay(1000);

  // 3. ИНИЦИАЛИЗАЦИЯ ШИН 1-WIRE (4 ШИНЫ)
  Serial.println("Инициализация шин 1-Wire...");
  sensorsA.begin();
  sensorsB.begin();
  sensorsC.begin();
  sensorsD.begin();

  sensors_init();

  // 4. ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРА
  Serial.println("Инициализация энкодера...");
  encoder_init();
  
  // 5. СОЗДАНИЕ ОБЪЕКТОВ FREERTOS
  initFreeRTOSObjects();

  // 6. НАЧАЛЬНАЯ ИНИЦИАЛИЗАЦИЯ СИСТЕМНЫХ ДАННЫХ
  sysData.mode = 0;
  sysData.needsRedraw = true;

  for (int i = 0; i < 6; i++) {
    sysData.temps[i] = 0.0f;
    sysData.colors[i] = 0;
  }

  guildBaseTemp = 0.0f;
  guildColorState = 0;

  // ========================================================================
  // ФЛАГИ ДЛЯ РАБОТЫ БЕЗ ДАТЧИКА ГИЛЬЗЫ
  // ========================================================================
  criticalError = !sensors[4].found;      // гильза теперь индекс 4
  systemInitialized = true;

  forceDisplayRedraw = true;
  lastDisplayMode = 0xFF;

  // 7. ФИНАЛЬНОЕ СООБЩЕНИЕ
  Serial.println("\n✅ Аппаратная часть инициализирована");
  Serial.println("📋 Введите HELP для списка команд");
  Serial.println("🎛️  Энкодер готов к работе");
  Serial.println(String(60, '=') + "\n");
  
  if (!sensors[4].found) {
    Serial.println("⚠️ ДАТЧИК ГИЛЬЗЫ НЕ ОБНАРУЖЕН");
    Serial.println("   → Переключение в MODE2 недоступно");
  }
}