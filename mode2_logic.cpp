#include "mode2_logic.h"
#include "eeprom_settings.h"  // <-- ДОБАВЛЕНО
#include "globals.h"
#include "system_config.h"
#include "measurement_core.h"

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
            
            // Получаем актуальные пороги из настроек
            float greenThreshold = settings_get_green_threshold();
            float yellowThreshold = settings_get_yellow_threshold();
            float hysteresis = settings_get_hysteresis();
            
            // Обновляем состояние с гистерезисом
            switch (guildColorState) {
                case 0: // ЗЕЛЁНЫЙ
                    if (diff >= (greenThreshold + hysteresis)) {
                        guildColorState = 1;
                        forceDisplayRedraw = true;
                    }
                    break;
                    
                case 1: // ЖЁЛТЫЙ
                    if (diff >= (yellowThreshold + hysteresis)) {
                        guildColorState = 2;
                        forceDisplayRedraw = true;
                    } else if (diff <= (greenThreshold - hysteresis)) {
                        guildColorState = 0;
                        forceDisplayRedraw = true;
                    }
                    break;
                    
                case 2: // КРАСНЫЙ
                    if (diff <= (yellowThreshold - hysteresis)) {
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