/**
 * ============================================================================
 * ФАЙЛ: display_engine.cpp
 * ДВИЖОК ОТОБРАЖЕНИЯ - УПРАВЛЕНИЕ TFT ДИСПЛЕЕМ
 *
 * ВЕРСИЯ: 7.0 (ДЕЛЬТА ПОЛНОСТЬЮ УДАЛЕНА)
 *
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - Удалены все упоминания дельты (lastDisplayDeltas, сравнения дельты и т.д.)
 * - При перерисовке датчиков проверяется только изменение температуры
 * - Упрощены функции очистки и отрисовки (только температура)
 * ============================================================================
 */

#include "display_engine.h"
#include "display_modes.h"
#include "display_common.h"
#include "globals.h"
#include "system_config.h"
#include "sensors.h"
#include "mode2_timer.h"
#include "mp3_player.h"
#include "menu_engine.h"

// Внешние объекты
extern TFT_eSPI tft;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t dataQueue;
extern QueueHandle_t eventQueue;

// ============================================================================
// ОПРЕДЕЛЕНИЕ ЦВЕТА ФОНА
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
// ПОЛНАЯ ПЕРЕРИСОВКА
// ============================================================================
void performFullDisplayRedraw() {
  uint16_t bgColor = getCurrentBackgroundColor();
  tft.fillScreen(bgColor);
  lastGlobalBgColor = bgColor;

  // Полный сброс кэша (только для температуры)
  for (int i = 0; i < 4; i++) {
    lastDisplayTemps[i] = -1000.0f;
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
  Serial.println("🖥️  Задача дисплея запущена (с поддержкой меню)");

  while (1) {
    // ========================================================================
    // 1. ОБРАБОТКА СОБЫТИЙ ЭНКОДЕРА (ВСЕГДА)
    // ========================================================================
    uint8_t encEvent;
    if (eventQueue != NULL && xQueueReceive(eventQueue, &encEvent, 0) == pdTRUE) {
      menu_handle_event((EncoderEvent_t)encEvent);
    }

    // ========================================================================
    // 2. ПРОВЕРКА ТАЙМАУТА МЕНЮ
    // ========================================================================
    menu_check_timeout();

    // ========================================================================
    // 3. ЕСЛИ АКТИВНО МЕНЮ - НЕ РИСУЕМ ГЛАВНЫЙ ЭКРАН
    // ========================================================================
    if (menu_is_active()) {
      SystemData_t dummy;
      while (xQueueReceive(dataQueue, &dummy, 0) == pdTRUE) {}
      vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
      continue;
    }

    // ========================================================================
    // 4. ПОЛУЧЕНИЕ ДАННЫХ ИЗ ОЧЕРЕДИ
    // ========================================================================
    SystemData_t receivedData;
    if (dataQueue != NULL && xQueueReceive(dataQueue, &receivedData, 0) == pdTRUE) {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(&sysData, &receivedData, sizeof(SystemData_t));
        xSemaphoreGive(dataMutex);
      }
    }

    // ========================================================================
    // 5. РЕЖИМ MODE1 (СТАБИЛИЗАЦИЯ)
    // ========================================================================
    if (sysData.mode == 0) {
      const uint16_t BG_COLOR = COLOR_BLUE;
      const uint16_t TEXT_COLOR = COLOR_WHITE;

      if (forceDisplayRedraw || !displayInitialized) {
        if (forceDisplayRedraw) {
          performFullDisplayRedraw();
        } else {
          tft.fillScreen(BG_COLOR);
          displayInitialized = true;
        }
        lastGlobalBgColor = BG_COLOR;
      }

      // Время
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

      // Отрисовка датчиков (только температура)
      for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;

        float temp = sysData.temps[i];
        int y = displayYPositions[i];

        if (!display_is_valid_temperature(temp)) continue;

        // Перерисовка только при изменении температуры
        if (fabs(temp - lastDisplayTemps[i]) >= 0.1f) {
          display_mode1_draw_sensor(i, y, temp);
          lastDisplayTemps[i] = temp;
        }
      }
    }

    // ========================================================================
    // 6. РЕЖИМ MODE2 (РАБОЧИЙ)
    // ========================================================================
    else {
      uint16_t bgColor = getCurrentBackgroundColor();
      uint16_t textColor = (bgColor == COLOR_RED) ? COLOR_WHITE : COLOR_BLACK;

      if (forceDisplayRedraw || !displayInitialized || bgColor != lastGlobalBgColor) {
        if (forceDisplayRedraw) {
          performFullDisplayRedraw();
        } else {
          tft.fillScreen(bgColor);
          lastGlobalBgColor = bgColor;
          for (int i = 0; i < 4; i++) {
            lastDisplayTemps[i] = -1000.0f;
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

      // Отрисовка датчиков (только температура)
      for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;

        float temp = sysData.temps[i];
        int y = displayYPositions[i];

        if (!display_is_valid_temperature(temp)) continue;

        if (fabs(temp - lastDisplayTemps[i]) >= 0.05f) {
          display_mode2_draw_sensor(i, y, temp, bgColor, textColor);
          lastDisplayTemps[i] = temp;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
  }
}

// ============================================================================
// СБРОС СОСТОЯНИЯ ДИСПЛЕЯ (ПРИ ПЕРЕКЛЮЧЕНИИ РЕЖИМОВ)
// ============================================================================
void resetDisplayState(uint8_t newMode) {
  Serial.printf("\n🔄 ПОЛНЫЙ СБРОС ДИСПЛЕЯ\n");
  Serial.printf("   Режим: %d -> %d\n", sysData.mode, newMode);

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    sysData.mode = newMode;
    sysData.needsRedraw = true;
    xSemaphoreGive(dataMutex);
  }

  forceDisplayRedraw = true;

  Mp3Command_t modeSound = { MP3_CMD_PLAY_TRACK, 3 };
  sendMP3Command(modeSound);

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

  // Сброс кэша (только для температуры)
  for (int i = 0; i < 4; i++) {
    lastDisplayTemps[i] = -1000.0f;
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