#include "display_modes.h"
#include "globals.h"
#include "system_config.h"
#include "mode2_timer.h"

// ============================================================================
// ВНЕШНИЕ ОБЪЯВЛЕНИЯ
// ============================================================================
extern TFT_eSPI tft;
extern SemaphoreHandle_t dataMutex;
extern float timeRefTemp;
extern uint32_t timeStartMs;
extern bool timeIsCounting;
extern uint8_t guildColorState;

// ============================================================================
// ОБЩИЕ ФУНКЦИИ (ВЫНЕСЕНЫ ИЗ display_engine.cpp)
// ============================================================================
bool display_is_valid_temperature(float temp) {
  if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) return false;
  if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) return false;
  if (temp <= TEMP_NO_DATA + 1.0f && temp >= TEMP_NO_DATA - 1.0f) return false;
  if (temp < -55.0f || temp > 125.0f) return false;
  return true;
}

void display_clear_temperature_area(int y, uint16_t bgColor) {
  tft.fillRect(10, y, maxTempWidth, bigFontHeight, bgColor);
}

void display_clear_delta_area(int y, const char* deltaStr, uint16_t bgColor) {
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

void display_draw_temperature(int y, float temp, uint16_t textColor, uint16_t bgColor) {
  tft.setTextFont(FONT_BIG);
  uint16_t actualTextColor = textColor;

  if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) {
    actualTextColor = COLOR_RED;
  } else if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) {
    actualTextColor = COLOR_YELLOW;
  }

  tft.setTextColor(actualTextColor, bgColor);
  int tempY = y + (RECT_HEIGHT - bigFontHeight) / 2;
  tft.setCursor(10, tempY);

  if (!display_is_valid_temperature(temp)) {
    tft.print("--.--");
  } else {
    if (temp < 10.0f && temp >= 0) tft.printf("0%.2f", temp);
    else tft.printf("%.2f", temp);
  }
  vTaskDelay(pdMS_TO_TICKS(1));
}

void display_draw_delta(int y, float delta, uint16_t textColor, uint16_t bgColor) {
  tft.setTextFont(FONT_DELTA);
  tft.setTextColor(textColor, bgColor);

  char deltaStr[8];
  if (delta < -100.0f || delta > 100.0f) {
    strcpy(deltaStr, "0.00");
  } else if (delta >= 0) {
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
  vTaskDelay(pdMS_TO_TICKS(1));
}

// ============================================================================
// РЕЖИМ СТАБИЛИЗАЦИИ (MODE1)
// ============================================================================
void display_mode1_draw_time() {
  const uint16_t BG_COLOR = COLOR_BLUE;
  const uint16_t TEXT_COLOR = COLOR_WHITE;

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

  // Кэширование строки времени (будет в display_engine.cpp)
  tft.fillRect(170, 0, 70, 30, BG_COLOR);
  tft.setTextFont(FONT_DELTA);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setCursor(170, 5);
  tft.print(currentTimeString);
  tft.setTextFont(FONT_BIG);
}

void display_mode1_draw_sensor(int idx, int y, float temp, float delta) {
  const uint16_t BG_COLOR = COLOR_BLUE;
  const uint16_t TEXT_COLOR = COLOR_WHITE;

  display_clear_temperature_area(y, BG_COLOR);
  display_draw_temperature(y, temp, TEXT_COLOR, BG_COLOR);

  char deltaStr[8];
  if (delta >= 0) sprintf(deltaStr, "+%.2f", delta);
  else sprintf(deltaStr, "%.2f", delta);

  display_clear_delta_area(y, deltaStr, BG_COLOR);
  display_draw_delta(y, delta, TEXT_COLOR, BG_COLOR);
}

void display_mode1_update() {
  // Эта функция будет вызываться из taskDisplay
  // Вся логика с кэшированием остаётся в display_engine.cpp
  // Сюда вынесена только отрисовка
}

// ============================================================================
// РАБОЧИЙ РЕЖИМ (MODE2)
// ============================================================================
void display_mode2_set_colors() {
  // Функция определения цветов (будет в display_engine.cpp)
}

void display_mode2_draw_sensor(int idx, int y, float temp, float delta) {
  // Будет вызываться с правильными цветами из display_engine.cpp
  uint16_t bgColor = getCurrentBackgroundColor();
  uint16_t textColor = (bgColor == COLOR_RED) ? COLOR_WHITE : COLOR_BLACK;

  display_clear_temperature_area(y, bgColor);
  display_draw_temperature(y, temp, textColor, bgColor);

  char deltaStr[8];
  if (delta >= 0) sprintf(deltaStr, "+%.2f", delta);
  else sprintf(deltaStr, "%.2f", delta);

  display_clear_delta_area(y, deltaStr, bgColor);
  display_draw_delta(y, delta, textColor, bgColor);
}

void display_mode2_update() {
  // Основная логика MODE2 (с цветами) остаётся в display_engine.cpp
  // Сюда вынесена только отрисовка
}