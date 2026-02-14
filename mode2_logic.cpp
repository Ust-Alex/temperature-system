#include "mode2_logic.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static uint32_t mode2_timer_start_ms = 0;
static bool mode2_timer_running = false;

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void mode2_set_base_temperature(float baseTemp) {
    guildBaseTemp = baseTemp;
}

float mode2_get_base_temperature() {
    return guildBaseTemp;
}

void mode2_update_color_state(float currentGuildTemp) {
    // Проверяем что мы в MODE2 и базовая температура установлена
    if (sysData.mode == 1 && guildBaseTemp != 0.0f) {
        if (isValidTemperature(currentGuildTemp)) {
            float diff = currentGuildTemp - guildBaseTemp;
            
            // Обновляем состояние с гистерезисом
            switch (guildColorState) {
                case 0: // ЗЕЛЁНЫЙ
                    if (diff >= (GREEN_TO_YELLOW_THRESHOLD + HYSTERESIS_VALUE)) {
                        guildColorState = 1;
                        forceDisplayRedraw = true;
                    }
                    break;
                    
                case 1: // ЖЁЛТЫЙ
                    if (diff >= (YELLOW_TO_RED_THRESHOLD + HYSTERESIS_VALUE)) {
                        guildColorState = 2;
                        forceDisplayRedraw = true;
                    } else if (diff <= (GREEN_TO_YELLOW_THRESHOLD - HYSTERESIS_VALUE)) {
                        guildColorState = 0;
                        forceDisplayRedraw = true;
                    }
                    break;
                    
                case 2: // КРАСНЫЙ
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

