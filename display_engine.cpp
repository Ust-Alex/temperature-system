/** * ФАЙЛ: display_engine.cpp
 * ДВИЖОК ОТОБРАЖЕНИЯ - УПРАВЛЕНИЕ TFT ДИСПЛЕЕМ
 * 
 * ВЕРСИЯ: 6.1 (ОПТИМИЗИРОВАННАЯ, НА ОСНОВЕ СТАРОЙ ЛОГИКИ)
 * 
 * ОТВЕТСТВЕННОСТЬ:
 * 1. Получение данных из очереди
 * 2. Кэширование значений (перерисовка только изменившегося)
 * 3. Быстрая смена фона без полной перерисовки
 * 4. Вызов функций отрисовки из модуля display_modes
 * 
 * ОПТИМИЗАЦИИ:
 * - Убраны vTaskDelay из отрисовки
 * - При смене цвета вызывается только fillScreen, без сброса кэша
 * - Кэширование lastDisplayTemps/lastDisplayDeltas (как в старом коде)
 * ============================================================================
 */

#include "display_engine.h"
#include "display_modes.h"
#include "display_common.h"
#include "globals.h"
#include "system_config.h"
#include "sensors.h"
#include "mode2_timer.h"


// Внешние объекты
extern TFT_eSPI tft;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t dataQueue;

// ============================================================================
// ОПРЕДЕЛЕНИЕ ЦВЕТА ФОНА (БЕЗ ИЗМЕНЕНИЙ)
// ============================================================================
uint16_t getCurrentBackgroundColor() {
  if (sysData.mode == 0) return COLOR_BLUE;
  switch (guildColorState) {
    case 0: return COLOR_GREEN;
    case 1: return COLOR_YELLOW;
    case 2: return COLOR_RED;
    default: return COLOR_BLUE;
  }
}

// ============================================================================
// ПОЛНАЯ ПЕРЕРИСОВКА (ТОЛЬКО ПО РЕАЛЬНОЙ НЕОБХОДИМОСТИ)
// ============================================================================
void performFullDisplayRedraw() {
  uint16_t bgColor = getCurrentBackgroundColor();
  tft.fillScreen(bgColor);
  lastGlobalBgColor = bgColor;

  // Полный сброс кэша
  for (int i = 0; i < 4; i++) {
    lastDisplayTemps[i] = -1000.0f;
    lastDisplayDeltas[i] = -1000.0f;
  }
  lastTimeString = "";
  lastMode2TimeString = "";

  forceDisplayRedraw = false;
  displayInitialized = true;

  Serial.printf("[DISPLAY] Полная перерисовка, режим: %d, цвет: %04X\n", 
                sysData.mode, bgColor);
}

// ============================================================================
// ЗАДАЧА ДИСПЛЕЯ (ОСНОВНОЙ ЦИКЛ)
// ============================================================================
void taskDisplay(void* pvParameters) {
  Serial.println("🖥️  Задача дисплея запущена (оптимизированная версия)");

  while (1) {
    // ========================================================================
    // 1. ПОЛУЧЕНИЕ ДАННЫХ ИЗ ОЧЕРЕДИ
    // ========================================================================
    SystemData_t receivedData;
    if (dataQueue != NULL && xQueueReceive(dataQueue, &receivedData, 0) == pdTRUE) {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(&sysData, &receivedData, sizeof(SystemData_t));
        xSemaphoreGive(dataMutex);
      }
    }

    // ========================================================================
    // 2. ОБРАБОТКА РЕЖИМА MODE1 (СТАБИЛИЗАЦИЯ)
    // ========================================================================
    if (sysData.mode == 0) {
      const uint16_t BG_COLOR = COLOR_BLUE;
      const uint16_t TEXT_COLOR = COLOR_WHITE;

      // Первый запуск или принудительная перерисовка
      if (forceDisplayRedraw || !displayInitialized) {
        if (forceDisplayRedraw) {
          performFullDisplayRedraw();
        } else {
          tft.fillScreen(BG_COLOR);
          displayInitialized = true;
        }
        lastGlobalBgColor = BG_COLOR;
      }

      // Обновление времени
      String currentTime = "00:00";
      if (timeIsCounting) {
        uint32_t elapsed = millis() - timeStartMs;
        uint32_t minutes = elapsed / 60000UL;
        uint8_t hours = minutes / 60;
        uint8_t mins = minutes % 60;
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", hours, mins);
        currentTime = String(buf);
      }

      if (currentTime != lastTimeString) {
        tft.fillRect(170, 0, 70, 30, BG_COLOR);
        tft.setTextFont(FONT_DELTA);
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.setCursor(170, 5);
        tft.print(currentTime);
        tft.setTextFont(FONT_BIG);
        lastTimeString = currentTime;
      }

      // Обновление датчиков
      for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;

        float temp = sysData.temps[i];
        float delta = sysData.deltas[i];
        int y = displayYPositions[i];

        if (!display_is_valid_temperature(temp)) continue;

        // Проверка кэша (порог 0.1°C для температуры, 0.01°C для дельты)
        bool needRedraw = false;
        if (fabs(temp - lastDisplayTemps[i]) >= 0.1f) needRedraw = true;
        else if (fabs(delta - lastDisplayDeltas[i]) >= 0.01f) needRedraw = true;

        if (!needRedraw) continue;

        // Отрисовка через модуль display_modes
        display_mode1_draw_sensor(i, y, temp, delta);

        // Сохранение в кэш
        lastDisplayTemps[i] = temp;
        lastDisplayDeltas[i] = delta;
      }
    }

    // ========================================================================
    // 3. ОБРАБОТКА РЕЖИМА MODE2 (РАБОЧИЙ)
    // ========================================================================
    else {
      uint16_t bgColor = getCurrentBackgroundColor();
      uint16_t textColor = (bgColor == COLOR_RED) ? COLOR_WHITE : COLOR_BLACK;

      // Смена цвета фона (без полной перерисовки)
      if (forceDisplayRedraw || !displayInitialized || bgColor != lastGlobalBgColor) {
        if (forceDisplayRedraw) {
          performFullDisplayRedraw();
        } else {
          tft.fillScreen(bgColor);
          lastGlobalBgColor = bgColor;
          // Сбрасываем кэш, чтобы всё перерисовалось
          for (int i = 0; i < 4; i++) {
            lastDisplayTemps[i] = -1000.0f;
            lastDisplayDeltas[i] = -1000.0f;
          }
          lastMode2TimeString = "";
          displayInitialized = true;

          const char* colorName = "Неизвестный";
          if (bgColor == COLOR_GREEN) colorName = "ЗЕЛЁНЫЙ";
          else if (bgColor == COLOR_YELLOW) colorName = "ЖЁЛТЫЙ";
          else if (bgColor == COLOR_RED) colorName = "КРАСНЫЙ";
          Serial.printf("[MODE2] Фон: %s (%04X)\n", colorName, bgColor);
        }
      }

      // Обновление времени MODE2
      String currentTime = mode2_timer_get_formatted();
      if (currentTime != lastMode2TimeString) {
        tft.fillRect(170, 0, 70, 30, bgColor);
        tft.setTextFont(FONT_DELTA);
        tft.setTextColor(textColor, bgColor);
        tft.setCursor(170, 5);
        tft.print(currentTime);
        tft.setTextFont(FONT_BIG);
        lastMode2TimeString = currentTime;
      }

      // Обновление датчиков
      for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;

        float temp = sysData.temps[i];
        float delta = sysData.deltas[i];
        int y = displayYPositions[i];

        if (!display_is_valid_temperature(temp)) continue;

        // Более чувствительный порог для MODE2 (0.05°C)
        bool needRedraw = false;
        if (fabs(temp - lastDisplayTemps[i]) >= 0.05f) needRedraw = true;
        else if (fabs(delta - lastDisplayDeltas[i]) >= 0.01f) needRedraw = true;

        if (!needRedraw) continue;

        // Отрисовка через модуль display_modes
        display_mode2_draw_sensor(i, y, temp, delta, bgColor, textColor);

        // Сохранение в кэш
        lastDisplayTemps[i] = temp;
        lastDisplayDeltas[i] = delta;
      }
    }

    // ========================================================================
    // 4. ЗАДЕРЖКА ДО СЛЕДУЮЩЕГО ЦИКЛА
    // ========================================================================
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
  }
}

// ============================================================================
// СБРОС СОСТОЯНИЯ ДИСПЛЕЯ (ПРИ ПЕРЕКЛЮЧЕНИИ РЕЖИМОВ)
// ============================================================================
void resetDisplayState(uint8_t newMode) {
  Serial.printf("\n🔄 ПОЛНЫЙ СБРОС ДИСПЛЕЯ\n");
  Serial.printf("   Режим: %d -> %d\n", sysData.mode, newMode);

  // Атомарное обновление режима
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    sysData.mode = newMode;
    sysData.needsRedraw = true;
    xSemaphoreGive(dataMutex);
  }

  forceDisplayRedraw = true;

  // Специфичная для режима логика
  if (newMode == 0) {
    mode2_timer_stop();
    timeRefTemp = 0.0f;
    timeStartMs = millis();
    timeIsCounting = false;
    Serial.println("   ✓ Таймер стабилизации MODE1 сброшен");

  } else if (newMode == 1) {
    mode2_timer_start();
    guildColorState = 0;
    Serial.println("   ✓ Цветовое состояние сброшено в ЗЕЛЁНЫЙ");

    // Сохранение базовой температуры гильзы
    if (sensors[3].found) {
      float currentGuildTemp = sensors[3].temp;
      if (display_is_valid_temperature(currentGuildTemp)) {
        guildBaseTemp = currentGuildTemp;
        Serial.printf("   ✓ Базовая температура гильзы: %.2f°C\n", guildBaseTemp);
      } else {
        guildBaseTemp = 20.0f;
        Serial.printf("   ⚠️  Базовая по умолчанию: %.1f°C\n", guildBaseTemp);
      }
    } else {
      guildBaseTemp = 20.0f;
      Serial.printf("   ⚠️  Гильза не найдена, базовая по умолчанию: %.1f°C\n", guildBaseTemp);
    }
  }

  // Сброс кэша и очереди
  for (int i = 0; i < 4; i++) {
    lastDisplayTemps[i] = -1000.0f;
    lastDisplayDeltas[i] = -1000.0f;
  }
  lastTimeString = "";
  lastMode2TimeString = "";
  displayInitialized = false;
  baseSaved = false;

  if (dataQueue != NULL) {
    xQueueReset(dataQueue);
  }

  Serial.println("   ✓ Сброс состояния завершен\n");
}