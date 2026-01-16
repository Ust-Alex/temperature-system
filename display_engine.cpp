#include "display_engine.h"

void resetDisplayState(uint8_t newMode) {
  Serial.printf("🔄 ПОЛНЫЙ СБРОС ДИСПЛЕЯ\n");
  Serial.printf("   Режим: %d -> %d\n", sysData.mode, newMode);

  forceDisplayRedraw = true;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    sysData.mode = newMode;
    sysData.needsRedraw = true;
    xSemaphoreGive(dataMutex);
  }

  lastDisplayMode = newMode;

  if (newMode == 0) {  // MODE1 - СТАБИЛИЗАЦИЯ
    Serial.println("   Настройка MODE1 (стабилизация):");
    
    // Останавливаем таймер MODE2
    mode2_timer_stop();
    
    // Сбрасываем таймер стабилизации MODE1
    timeRefTemp = 0.0f;
    timeStartMs = millis();
    timeIsCounting = false;
    Serial.println("   ✓ Таймер стабилизации сброшен");

    // Сбрасываем кэш дисплея
    for (int i = 0; i < 4; i++) {
      lastDisplayTemps[i] = -1000.0f;
      lastDisplayDeltas[i] = -1000.0f;
    }
    Serial.println("   ✓ Кэш дисплея очищен");

  } else if (newMode == 1) {  // MODE2 - РАБОЧИЙ РЕЖИМ
    Serial.println("   Настройка MODE2 (рабочий режим):");
    
    // Запускаем таймер MODE2
    mode2_timer_start();
    
    // Сбрасываем цветовое состояние
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
        Serial.printf("   ⚠️  Текущая температура некорректна (%.1f°C)\n", currentGuildTemp);
        Serial.printf("   ✓ Установлена базовая по умолчанию: %.1f°C\n", guildBaseTemp);
      }
    } else {
      guildBaseTemp = 20.0f;
      Serial.println("   ⚠️  Гильза не найдена!");
      Serial.printf("   ✓ Установлена базовая по умолчанию: %.1f°C\n", guildBaseTemp);
    }

    // Сбрасываем кэш дисплея
    for (int i = 0; i < 4; i++) {
      lastDisplayTemps[i] = -1000.0f;
      lastDisplayDeltas[i] = -1000.0f;
    }
    Serial.println("   ✓ Кэш дисплея очищен");
  }

  baseSaved = false;

  if (dataQueue != NULL) {
    xQueueReset(dataQueue);
    Serial.println("   ✓ Очередь данных очищена");
  }
  Serial.print("360 - ");
  Serial.println(String(50, '=') + "\n");
}

void performFullDisplayRedraw() {
  uint16_t bgColor = getCurrentBackgroundColor();
  tft.fillScreen(bgColor);
  lastGlobalBgColor = bgColor;

  Serial.printf("[DISPLAY] Полная перерисовка экрана, цвет: %04X\n", bgColor);

  forceDisplayRedraw = false;

  for (int i = 0; i < 4; i++) {
    lastDisplayTemps[i] = -1000.0f;
    lastDisplayDeltas[i] = -1000.0f;
  }
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

void updateDisplayMODE1() {
  const uint16_t BG_COLOR = COLOR_BLUE;
  const uint16_t TEXT_COLOR = COLOR_WHITE;

  static bool displayInitialized = false;
  static String lastTimeString = "";

  if (forceDisplayRedraw || !displayInitialized) {
    if (forceDisplayRedraw) {
      performFullDisplayRedraw();
    } else {
      tft.fillScreen(BG_COLOR);
    }
    displayInitialized = true;
    Serial.println("[MODE1] Дисплей инициализирован (полная перерисовка)");
  }

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

  if (currentTimeString != lastTimeString) {
    tft.fillRect(170, 0, 70, 30, BG_COLOR);

    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setCursor(170, 5);
    tft.print(currentTimeString);
    tft.setTextFont(FONT_BIG);

    lastTimeString = currentTimeString;
  }

  for (int i = 0; i < 4; i++) {
    if (!sensors[i].found) continue;

    float temp = sysData.temps[i];
    float delta = sysData.deltas[i];
    int y = displayYPositions[i];

    if (!isValidTemperature(temp)) {
      continue;
    }

    bool needRedraw = false;

    if (fabs(temp - lastDisplayTemps[i]) >= 0.1f) {
      needRedraw = true;
    } else if (fabs(delta - lastDisplayDeltas[i]) >= 0.01f) {
      needRedraw = true;
    }

    if (!needRedraw) continue;

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

    lastDisplayTemps[i] = temp;
    lastDisplayDeltas[i] = delta;
  }
}

void updateDisplayMODE2_Common(uint16_t bgColor, uint16_t textColor) {
  // Статические переменные для отслеживания состояния дисплея
  static bool displayInitialized = false;
  static String lastMode2TimeString = "";  // НОВАЯ переменная для кэширования времени

  // ===== 1. ПРОВЕРКА НЕОБХОДИМОСТИ ПОЛНОЙ ПЕРЕРИСОВКИ =====
  if (forceDisplayRedraw || !displayInitialized || bgColor != lastGlobalBgColor) {
    if (forceDisplayRedraw) {
      performFullDisplayRedraw();
    } else {
      // Просто меняем цвет фона
      tft.fillScreen(bgColor);
      lastGlobalBgColor = bgColor;

      // Сбрасываем кэш, чтобы все перерисовалось
      for (int i = 0; i < 4; i++) {
        lastDisplayTemps[i] = -1000.0f;
        lastDisplayDeltas[i] = -1000.0f;
      }
    }

    displayInitialized = true;
    lastMode2TimeString = "";  // Сбрасываем кэш времени при перерисовке

    // Логируем смену цвета
    const char* colorName = "Неизвестный";
    if (bgColor == COLOR_GREEN) colorName = "ЗЕЛЁНЫЙ";
    else if (bgColor == COLOR_YELLOW) colorName = "ЖЁЛТЫЙ";
    else if (bgColor == COLOR_RED) colorName = "КРАСНЫЙ";

    Serial.printf("[MODE2] Фон установлен: %s (%04X)\n", colorName, bgColor);
  }

  // ===== 2. ОТОБРАЖЕНИЕ ВРЕМЕНИ MODE2 =====
  String currentTimeString = mode2_timer_get_formatted();

  // Отрисовываем время только если оно изменилось
  if (currentTimeString != lastMode2TimeString) {
    // Очищаем область времени (правый верхний угол)
    tft.fillRect(170, 0, 70, 30, bgColor);

    // Рисуем новое время
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(170, 5);
    tft.print(currentTimeString);
    tft.setTextFont(FONT_BIG);

    lastMode2TimeString = currentTimeString;
  }

  // ===== 3. ОБНОВЛЕНИЕ ТЕМПЕРАТУР И ΔT =====
  for (int i = 0; i < 4; i++) {
    // Пропускаем ненайденные датчики
    if (!sensors[i].found) continue;

    float temp = sysData.temps[i];
    float delta = sysData.deltas[i];
    int y = displayYPositions[i];

    // Пропускаем датчики с ошибками
    if (!isValidTemperature(temp)) {
      continue;
    }

    // ===== 3.1. ПРОВЕРКА: НУЖНО ЛИ ПЕРЕРИСОВЫВАТЬ? =====
    bool needRedraw = false;

    // Более чувствительный порог для MODE2 (0.05°C вместо 0.1°C)
    if (fabs(temp - lastDisplayTemps[i]) >= 0.05f) {
      needRedraw = true;
    }
    // Или изменение дельты (порог 0.01°C)
    else if (fabs(delta - lastDisplayDeltas[i]) >= 0.01f) {
      needRedraw = true;
    }

    // Если изменения недостаточны - пропускаем
    if (!needRedraw) continue;

    // ===== 3.2. ПЕРЕРИСОВКА ЭТОГО ДАТЧИКА =====
    // 3.2.1. Температура
    clearTemperatureArea(y, bgColor);
    drawTemperature(y, temp, textColor, bgColor);

    // 3.2.2. ΔT
    char deltaStr[8];
    if (delta >= 0) {
      sprintf(deltaStr, "+%.2f", delta);
    } else {
      sprintf(deltaStr, "%.2f", delta);
    }

    clearDeltaArea(y, deltaStr, bgColor);
    drawDelta(y, delta, textColor, bgColor);

    // ===== 3.3. СОХРАНЕНИЕ ТЕКУЩИХ ЗНАЧЕНИЙ В КЭШ =====
    lastDisplayTemps[i] = temp;
    lastDisplayDeltas[i] = delta;
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