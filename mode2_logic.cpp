#include "mode2_logic.h"

static uint32_t mode2_timer_start_ms = 0;
static bool mode2_timer_running = false;

void mode2_set_base_temperature(float baseTemp) {
  guildBaseTemp = baseTemp;
}

float mode2_get_base_temperature() {
  return guildBaseTemp;
}

void mode2_update_color_state(float currentGuildTemp) {
  if (sysData.mode == 1 && guildBaseTemp != 0.0f) {
    if (isValidTemperature(currentGuildTemp)) {
      float diff = currentGuildTemp - guildBaseTemp;

      switch (guildColorState) {
        case 0:
          if (diff >= (GREEN_TO_YELLOW_THRESHOLD + HYSTERESIS_VALUE)) {
            guildColorState = 1;
            forceDisplayRedraw = true;
          }
          break;
        case 1:
          if (diff >= (YELLOW_TO_RED_THRESHOLD + HYSTERESIS_VALUE)) {
            guildColorState = 2;
            forceDisplayRedraw = true;
          } else if (diff <= (GREEN_TO_YELLOW_THRESHOLD - HYSTERESIS_VALUE)) {
            guildColorState = 0;
            forceDisplayRedraw = true;
          }
          break;
        case 2:
          if (diff <= (YELLOW_TO_RED_THRESHOLD - HYSTERESIS_VALUE)) {
            guildColorState = 1;
            forceDisplayRedraw = true;
          }
          break;
      }
    }
  }
}

uint8_t mode2_get_current_color_state() {
  return guildColorState;
}

// ========== НОВЫЕ ФУНКЦИИ ТАЙМЕРА MODE2 ==========

void mode2_timer_start() {
  mode2_timer_start_ms = millis();
  mode2_timer_running = true;
  Serial.println("[MODE2] Таймер запущен");
}

void mode2_timer_stop() {
  mode2_timer_running = false;
  Serial.println("[MODE2] Таймер остановлен");
}

void mode2_timer_reset() {
  mode2_timer_start_ms = millis();
  Serial.println("[MODE2] Таймер сброшен");
}

String mode2_timer_get_formatted() {
  if (!mode2_timer_running) return "00:00";
  
  uint32_t elapsed = millis() - mode2_timer_start_ms;
  uint32_t minutes = elapsed / 60000UL;
  uint8_t hours = minutes / 60;
  uint8_t mins = minutes % 60;
  
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", hours, mins);
  return String(buffer);
}

bool mode2_timer_is_active() {
  return mode2_timer_running;
}