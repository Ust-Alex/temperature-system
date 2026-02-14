/**
 * ============================================================================
 * ФАЙЛ: display_engine.cpp
 * ДВИЖОК ОТОБРАЖЕНИЯ - УПРАВЛЕНИЕ TFT ДИСПЛЕЕМ
 * 
 * ВЕРСИЯ: 6.0 (С МОДУЛЬНОЙ АРХИТЕКТУРОЙ)
 * 
 * ОТВЕТСТВЕННОСТЬ:
 * 1. Получение данных из очереди
 * 2. Кэширование значений для оптимизации перерисовки
 * 3. Вызов функций отрисовки из display_modes
 * 4. Управление полной перерисовкой при смене режима
 * 
 * ВАЖНО: Этот файл НЕ СОДЕРЖИТ код отрисовки!
 *        Вся отрисовка в display_modes и display_common
 * ============================================================================
 */

#include "display_engine.h"
#include "display_modes.h"
#include "display_common.h"
#include "globals.h"
#include "system_config.h"
#include "sensors.h"
#include "mode2_timer.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ КЭШИРОВАНИЯ
// ============================================================================
static float cachedTemps[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };
static float cachedDeltas[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };
static String cachedTimeString = "";
static String cachedMode2TimeString = "";
static uint8_t cachedDisplayMode = 0xFF;
static uint16_t cachedBgColor = 0xFFFF;
static bool displayInitialized = false;

// Внешние объекты
extern TFT_eSPI tft;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t dataQueue;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

uint16_t getCurrentBackgroundColor() {
  if (sysData.mode == 0) {
    return COLOR_BLUE;
  }
  
  switch (guildColorState) {
    case 0: return COLOR_GREEN;
    case 1: return COLOR_YELLOW;
    case 2: return COLOR_RED;
    default: return COLOR_BLUE;
  }
}

void performFullDisplayRedraw() {
  uint16_t bgColor = getCurrentBackgroundColor();
  tft.fillScreen(bgColor);
  
  // Полный сброс кэша
  for (int i = 0; i < 4; i++) {
    cachedTemps[i] = -1000.0f;
    cachedDeltas[i] = -1000.0f;
  }
  cachedTimeString = "";
  cachedMode2TimeString = "";
  cachedDisplayMode = sysData.mode;
  cachedBgColor = bgColor;
  
  Serial.printf("[DISPLAY] Полная перерисовка, режим: %d, цвет: %04X\n", 
                sysData.mode, bgColor);
}

// ============================================================================
// ЗАДАЧА ДИСПЛЕЯ
// ============================================================================
void taskDisplay(void* pvParameters) {
  Serial.println("🖥️  Задача дисплея запущена (модульная архитектура)");
  
  uint32_t lastHeartbeat = 0;
  
  while (1) {
    uint32_t currentMillis = millis();
    
    // Heartbeat для отладки (раз в 30 секунд)
    if (currentMillis - lastHeartbeat > 30000) {
      // Serial.printf("[DISPLAY] Heartbeat\n");
      lastHeartbeat = currentMillis;
    }
    
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
    // 2. ПРОВЕРКА НЕОБХОДИМОСТИ ПОЛНОЙ ПЕРЕРИСОВКИ
    // ========================================================================
    bool needFullRedraw = forceDisplayRedraw || !displayInitialized;
    
    if (sysData.mode != cachedDisplayMode) {
      needFullRedraw = true;
    } else if (sysData.mode == 1 && cachedBgColor != getCurrentBackgroundColor()) {
      needFullRedraw = true;
    }
    
    if (needFullRedraw) {
      performFullDisplayRedraw();
      forceDisplayRedraw = false;
      displayInitialized = true;
    }
    
    // ========================================================================
    // 3. ОБНОВЛЕНИЕ ЭКРАНА В ЗАВИСИМОСТИ ОТ РЕЖИМА
    // ========================================================================
    if (sysData.mode == 0) {
      // ======================================================================
      // РЕЖИМ MODE1 (СТАБИЛИЗАЦИЯ)
      // ======================================================================
      
      // Обновление времени
      String currentTime = "";
      if (timeIsCounting) {
        uint32_t elapsed = millis() - timeStartMs;
        uint32_t minutes = elapsed / 60000UL;
        uint8_t hours = minutes / 60;
        uint8_t mins = minutes % 60;
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", hours, mins);
        currentTime = String(buf);
      } else {
        currentTime = "00:00";
      }
      
      if (currentTime != cachedTimeString) {
        display_mode1_draw_time();
        cachedTimeString = currentTime;
      }
      
      // Обновление датчиков
      for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;
        
        float temp = sysData.temps[i];
        float delta = sysData.deltas[i];
        int y = displayYPositions[i];
        
        if (!display_is_valid_temperature(temp)) continue;
        
        // Проверка кэша
        bool needRedraw = false;
        if (fabs(temp - cachedTemps[i]) >= 0.1f) needRedraw = true;
        else if (fabs(delta - cachedDeltas[i]) >= 0.01f) needRedraw = true;
        
        if (needRedraw) {
          display_mode1_draw_sensor(i, y, temp, delta);
          cachedTemps[i] = temp;
          cachedDeltas[i] = delta;
        }
      }
      
    } else {
      // ======================================================================
      // РЕЖИМ MODE2 (РАБОЧИЙ)
      // ======================================================================
      
      uint16_t bgColor = getCurrentBackgroundColor();
      uint16_t textColor = (bgColor == COLOR_RED) ? COLOR_WHITE : COLOR_BLACK;
      
      // Обновление времени MODE2
      String currentTime = mode2_timer_get_formatted();
      if (currentTime != cachedMode2TimeString) {
        tft.fillRect(170, 0, 70, 30, bgColor);
        tft.setTextFont(FONT_DELTA);
        tft.setTextColor(textColor, bgColor);
        tft.setCursor(170, 5);
        tft.print(currentTime);
        tft.setTextFont(FONT_BIG);
        cachedMode2TimeString = currentTime;
      }
      
      // Обновление датчиков
      for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;
        
        float temp = sysData.temps[i];
        float delta = sysData.deltas[i];
        int y = displayYPositions[i];
        
        if (!display_is_valid_temperature(temp)) continue;
        
        // Более чувствительный порог для MODE2
        bool needRedraw = false;
        if (fabs(temp - cachedTemps[i]) >= 0.05f) needRedraw = true;
        else if (fabs(delta - cachedDeltas[i]) >= 0.01f) needRedraw = true;
        
        if (needRedraw) {
          display_mode2_draw_sensor(i, y, temp, delta, bgColor, textColor);
          cachedTemps[i] = temp;
          cachedDeltas[i] = delta;
        }
      }
    }
    
    // ========================================================================
    // 4. ЗАДЕРЖКА ДО СЛЕДУЮЩЕГО ЦИКЛА
    // ========================================================================
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
  }
}

// ============================================================================
// СБРОС СОСТОЯНИЯ ДИСПЛЕЯ
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
  
  // Сброс кэша
  for (int i = 0; i < 4; i++) {
    cachedTemps[i] = -1000.0f;
    cachedDeltas[i] = -1000.0f;
  }
  cachedTimeString = "";
  cachedMode2TimeString = "";
  cachedDisplayMode = 0xFF;
  displayInitialized = false;
  
  baseSaved = false;
  
  // Очистка очереди
  if (dataQueue != NULL) {
    xQueueReset(dataQueue);
  }
  
  Serial.println("   ✓ Сброс состояния завершен\n");
}