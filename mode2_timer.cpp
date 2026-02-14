#include "mode2_timer.h"
#include "globals.h"
#include "system_config.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// ============================================================================
static uint32_t timerStartMs = 0;        // Время запуска таймера
static bool timerRunning = false;        // Флаг: таймер запущен
static uint32_t lastUpdateMs = 0;        // Время последнего обновления (для оптимизации)
static char formattedTime[6] = "00:00";  // Кэшированная строка времени

// ============================================================================
// УПРАВЛЕНИЕ ТАЙМЕРОМ
// ============================================================================
void mode2_timer_start() {
  if (!timerRunning) {
    timerStartMs = millis();
    timerRunning = true;
    lastUpdateMs = timerStartMs;
    Serial.println("[MODE2_TIMER] Таймер запущен");
  }
}

void mode2_timer_stop() {
  if (timerRunning) {
    timerRunning = false;
    Serial.println("[MODE2_TIMER] Таймер остановлен");
  }
}

void mode2_timer_reset() {
  timerStartMs = millis();
  lastUpdateMs = timerStartMs;
  // Не меняем состояние running
  Serial.println("[MODE2_TIMER] Таймер сброшен");
}

// ============================================================================
// ОБНОВЛЕНИЕ И ФОРМАТИРОВАНИЕ
// ============================================================================
void mode2_timer_update() {
  // Обновляем не чаще чем раз в 500 мс (чтобы не дёргать лишний раз)
  if (timerRunning && (millis() - lastUpdateMs >= 500)) {
    lastUpdateMs = millis();
    // Форматирование будет выполнено при запросе, здесь только обновляем флаг
  }
}

String mode2_timer_get_formatted() {
  if (!timerRunning) return "00:00";
  
  uint32_t elapsed = millis() - timerStartMs;
  uint32_t totalMinutes = elapsed / 60000UL;
  uint8_t hours = totalMinutes / 60;
  uint8_t minutes = totalMinutes % 60;
  
  // Обновляем кэш только при изменении
  static uint8_t lastHours = 0xFF;
  static uint8_t lastMinutes = 0xFF;
  
  if (hours != lastHours || minutes != lastMinutes) {
    snprintf(formattedTime, sizeof(formattedTime), "%02d:%02d", hours, minutes);
    lastHours = hours;
    lastMinutes = minutes;
  }
  
  return String(formattedTime);
}

uint32_t mode2_timer_get_seconds() {
  if (!timerRunning) return 0;
  return (millis() - timerStartMs) / 1000UL;
}

bool mode2_timer_is_active() {
  return timerRunning;
}