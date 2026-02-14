#include "display_engine.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ТОЛЬКО ДЛЯ КЭШИРОВАНИЯ ДИСПЛЕЯ
// Эти переменные используются только в этом файле для оптимизации отрисовки
// ============================================================================
static float cachedTemps[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };
static float cachedDeltas[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };
static String cachedTimeString = "";
static String cachedMode2TimeString = "";
static uint8_t cachedDisplayMode = 0xFF;
static uint16_t cachedBgColor = 0xFFFF;
static bool displayInitialized = false;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (без изменений)
// ============================================================================

void performFullDisplayRedraw() {
    uint16_t bgColor = getCurrentBackgroundColor();
    tft.fillScreen(bgColor);
    cachedBgColor = bgColor;
    
    // Сбрасываем кэш
    for (int i = 0; i < 4; i++) {
        cachedTemps[i] = -1000.0f;
        cachedDeltas[i] = -1000.0f;
    }
    cachedTimeString = "";
    cachedMode2TimeString = "";
    cachedDisplayMode = sysData.mode;
    
    Serial.printf("[DISPLAY] Полная перерисовка, цвет: %04X\n", bgColor);
}

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

void clearTemperatureArea(int y, uint16_t bgColor) {
    tft.fillRect(10, y, maxTempWidth, bigFontHeight, bgColor);
}

void clearDeltaArea(int y, const char* deltaStr, uint16_t bgColor) {
    tft.setTextFont(FONT_DELTA);
    int deltaWidth = tft.textWidth(deltaStr);
    int deltaX = 240 - deltaWidth - 10;
    int deltaY = y + (RECT_HEIGHT - deltaFontHeight);
    
    if (deltaY + deltaFontHeight > y + RECT_HEIGHT) {
        deltaY = y + RECT_HEIGHT - deltaFontHeight - 5;
    }
    
    tft.fillRect(deltaX - 5, deltaY, deltaWidth + 10, deltaFontHeight, bgColor);
    tft.setTextFont(FONT_BIG);
}

void drawTemperature(int y, float temp, uint16_t textColor, uint16_t bgColor) {
    tft.setTextFont(FONT_BIG);
    tft.setTextColor(textColor, bgColor);
    
    int tempY = y + (RECT_HEIGHT - bigFontHeight) / 2;
    tft.setCursor(10, tempY);
    
    if (temp < 10.0f && temp >= 0) {
        tft.printf("0%.2f", temp);
    } else {
        tft.printf("%.2f", temp);
    }
}

void drawDelta(int y, float delta, uint16_t textColor, uint16_t bgColor) {
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(textColor, bgColor);
    
    char deltaStr[8];
    if (delta >= 0) {
        sprintf(deltaStr, "+%.2f", delta);
    } else {
        sprintf(deltaStr, "%.2f", delta);
    }
    
    int deltaWidth = tft.textWidth(deltaStr);
    int deltaX = 240 - deltaWidth - 10;
    int deltaY = y + (RECT_HEIGHT - deltaFontHeight);
    
    if (deltaY + deltaFontHeight > y + RECT_HEIGHT) {
        deltaY = y + RECT_HEIGHT - deltaFontHeight - 5;
    }
    
    tft.setCursor(deltaX, deltaY);
    tft.print(deltaStr);
    tft.setTextFont(FONT_BIG);
}

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ ОБНОВЛЕНИЯ ДИСПЛЕЯ
// ============================================================================

void updateDisplayMODE1() {
    const uint16_t BG_COLOR = COLOR_BLUE;
    const uint16_t TEXT_COLOR = COLOR_WHITE;
    
    // 1. Полная перерисовка при необходимости
    if (forceDisplayRedraw || !displayInitialized || cachedDisplayMode != sysData.mode) {
        performFullDisplayRedraw();
        forceDisplayRedraw = false;
        displayInitialized = true;
        cachedDisplayMode = sysData.mode;
    }
    
    // 2. Отображение времени стабилизации
    String currentTimeString = "00:00";
    if (timeIsCounting) {
        uint32_t elapsedMillis = millis() - timeStartMs;
        uint32_t elapsedMinutes = elapsedMillis / 60000UL;
        uint8_t hours = elapsedMinutes / 60;
        uint8_t minutes = elapsedMinutes % 60;
        
        char timeBuffer[6];
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hours, minutes);
        currentTimeString = String(timeBuffer);
    }
    
    // Обновляем время только при изменении
    if (currentTimeString != cachedTimeString) {
        tft.fillRect(170, 0, 70, 30, BG_COLOR);
        tft.setTextFont(FONT_DELTA);
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.setCursor(170, 5);
        tft.print(currentTimeString);
        tft.setTextFont(FONT_BIG);
        cachedTimeString = currentTimeString;
    }
    
    // 3. Отрисовка температур и дельт
    for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;
        
        float temp = sysData.temps[i];
        float delta = sysData.deltas[i];
        int y = displayYPositions[i];
        
        if (!isValidTemperature(temp)) continue;
        
        // Проверка: нужно ли перерисовывать этот датчик?
        bool needRedraw = false;
        if (fabs(temp - cachedTemps[i]) >= 0.1f) {
            needRedraw = true;
        } else if (fabs(delta - cachedDeltas[i]) >= 0.01f) {
            needRedraw = true;
        }
        
        if (!needRedraw) continue;
        
        // Перерисовка датчика
        clearTemperatureArea(y, BG_COLOR);
        drawTemperature(y, temp, TEXT_COLOR, BG_COLOR);
        
        char deltaStr[8];
        if (delta >= 0) {
            sprintf(deltaStr, "+%.2f", delta);
        } else {
            sprintf(deltaStr, "%.2f", delta);
        }
        
        clearDeltaArea(y, deltaStr, BG_COLOR);
        drawDelta(y, delta, TEXT_COLOR, BG_COLOR);
        
        // Сохраняем в кэш
        cachedTemps[i] = temp;
        cachedDeltas[i] = delta;
    }
}

void updateDisplayMODE2_Common(uint16_t bgColor, uint16_t textColor) {
    // 1. Проверка необходимости полной перерисовки
    if (forceDisplayRedraw || !displayInitialized || cachedDisplayMode != sysData.mode || cachedBgColor != bgColor) {
        tft.fillScreen(bgColor);
        cachedBgColor = bgColor;
        cachedDisplayMode = sysData.mode;
        displayInitialized = true;
        forceDisplayRedraw = false;
        
        // Сбрасываем кэш
        for (int i = 0; i < 4; i++) {
            cachedTemps[i] = -1000.0f;
            cachedDeltas[i] = -1000.0f;
        }
        cachedMode2TimeString = "";
        
        const char* colorName = "Неизвестный";
        if (bgColor == COLOR_GREEN) colorName = "ЗЕЛЁНЫЙ";
        else if (bgColor == COLOR_YELLOW) colorName = "ЖЁЛТЫЙ";
        else if (bgColor == COLOR_RED) colorName = "КРАСНЫЙ";
        
        Serial.printf("[MODE2] Фон установлен: %s (%04X)\n", colorName, bgColor);
    }
    
    // 2. Отображение времени MODE2
    String currentTimeString = mode2_timer_get_formatted();
    if (currentTimeString != cachedMode2TimeString) {
        tft.fillRect(170, 0, 70, 30, bgColor);
        tft.setTextFont(FONT_DELTA);
        tft.setTextColor(textColor, bgColor);
        tft.setCursor(170, 5);
        tft.print(currentTimeString);
        tft.setTextFont(FONT_BIG);
        cachedMode2TimeString = currentTimeString;
    }
    
    // 3. Отрисовка температур и дельт
    for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;
        
        float temp = sysData.temps[i];
        float delta = sysData.deltas[i];
        int y = displayYPositions[i];
        
        if (!isValidTemperature(temp)) continue;
        
        // Более чувствительный порог для MODE2
        bool needRedraw = false;
        if (fabs(temp - cachedTemps[i]) >= 0.05f) {
            needRedraw = true;
        } else if (fabs(delta - cachedDeltas[i]) >= 0.01f) {
            needRedraw = true;
        }
        
        if (!needRedraw) continue;
        
        // Перерисовка
        clearTemperatureArea(y, bgColor);
        drawTemperature(y, temp, textColor, bgColor);
        
        char deltaStr[8];
        if (delta >= 0) {
            sprintf(deltaStr, "+%.2f", delta);
        } else {
            sprintf(deltaStr, "%.2f", delta);
        }
        
        clearDeltaArea(y, deltaStr, bgColor);
        drawDelta(y, delta, textColor, bgColor);
        
        // Кэширование
        cachedTemps[i] = temp;
        cachedDeltas[i] = delta;
    }
}

void updateDisplayMODE2_GREEN() {
    updateDisplayMODE2_Common(COLOR_GREEN, COLOR_BLACK);
}

void updateDisplayMODE2_YELLOW() {
    updateDisplayMODE2_Common(COLOR_YELLOW, COLOR_BLACK);
}

void updateDisplayMODE2_RED() {
    updateDisplayMODE2_Common(COLOR_RED, COLOR_WHITE);
}

// ============================================================================
// ФУНКЦИЯ СБРОСА СОСТОЯНИЯ ДИСПЛЕЯ
// ============================================================================
void resetDisplayState(uint8_t newMode) {
    Serial.printf("🔄 ПОЛНЫЙ СБРОС ДИСПЛЕЯ\n");
    Serial.printf("   Режим: %d -> %d\n", sysData.mode, newMode);
    
    // АТОМАРНАЯ операция: все изменения делаем под мьютексом
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        sysData.mode = newMode;
        sysData.needsRedraw = true;
        forceDisplayRedraw = true;
        xSemaphoreGive(dataMutex);
    }
    
    // Обновляем глобальные переменные (не требуют мьютекса, т.к. используются только здесь)
    lastDisplayMode = newMode;
    
    // Логика сброса в зависимости от режима
    if (newMode == 0) {
        mode2_timer_stop();
        timeRefTemp = 0.0f;
        timeStartMs = millis();
        timeIsCounting = false;
        Serial.println("   ✓ Таймер стабилизации сброшен");
    } else if (newMode == 1) {
        mode2_timer_start();
        guildColorState = 0;
        Serial.println("   ✓ Цветовое состояние сброшено в ЗЕЛЁНЫЙ");
        
        // Сохраняем текущую температуру гильзы
        if (sensors[3].found) {
            float currentGuildTemp = sysData.temps[3];
            if (isValidTemperature(currentGuildTemp)) {
                guildBaseTemp = currentGuildTemp;
                Serial.printf("   ✓ Базовая температура гильзы: %.2f°C\n", guildBaseTemp);
            } else {
                guildBaseTemp = 20.0f;
                Serial.printf("   ⚠️  Установлена базовая по умолчанию: %.1f°C\n", guildBaseTemp);
            }
        } else {
            guildBaseTemp = 20.0f;
            Serial.printf("   ⚠️  Установлена базовая по умолчанию: %.1f°C\n", guildBaseTemp);
        }
    }
    
    // Сбрасываем кэш дисплея
    for (int i = 0; i < 4; i++) {
        cachedTemps[i] = -1000.0f;
        cachedDeltas[i] = -1000.0f;
    }
    cachedTimeString = "";
    cachedMode2TimeString = "";
    cachedDisplayMode = 0xFF;
    displayInitialized = false;
    
    baseSaved = false;
    
    // Очищаем очередь данных
    if (dataQueue != NULL) {
        xQueueReset(dataQueue);
    }
    
    Serial.println("   ✓ Сброс состояния завершен\n");
}