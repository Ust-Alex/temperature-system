/**
 * ============================================================================
 * ФАЙЛ: mode2_logic.cpp
 * ЛОГИКА РАБОЧЕГО РЕЖИМА (MODE2)
 * ВЕРСИЯ: 3.0 (ГИЛЬЗА ИНДЕКС 4)
 * ============================================================================
 */

#include "mode2_logic.h"
#include "eeprom_settings.h"
#include "globals.h"
#include "system_config.h"
#include "measurement_core.h"
#include "mp3_player.h"
#include "mode2_sounds.h"

void mode2_set_base_temperature(float baseTemp) {
    guildBaseTemp = baseTemp;
}

float mode2_get_base_temperature() {
    return guildBaseTemp;
}

void mode2_update_color_state(float currentGuildTemp) {
    // Гильза теперь индекс 4
    if (!sensors[4].found) {
        return;
    }
    
    if (sysData.mode == 1 && guildBaseTemp != 0.0f) {
        if (isValidTemperature(currentGuildTemp)) {
            float diff = currentGuildTemp - guildBaseTemp;
            float greenThreshold = settings_get_green_threshold();
            float yellowThreshold = settings_get_yellow_threshold();
            float hysteresis = settings_get_hysteresis();
            
            uint8_t oldState = guildColorState;
            
            switch (guildColorState) {
                case 0:
                    if (diff >= (greenThreshold + hysteresis)) {
                        guildColorState = 1;
                        forceDisplayRedraw = true;
                    }
                    break;
                case 1:
                    if (diff >= (yellowThreshold + hysteresis)) {
                        guildColorState = 2;
                        forceDisplayRedraw = true;
                    } else if (diff <= (greenThreshold - hysteresis)) {
                        guildColorState = 0;
                        forceDisplayRedraw = true;
                    }
                    break;
                case 2:
                    if (diff <= (yellowThreshold - hysteresis)) {
                        guildColorState = 1;
                        forceDisplayRedraw = true;
                    }
                    break;
            }
            
            if (oldState != guildColorState) {
                if (guildColorState == 1) {
                    Mp3Command_t yellowSound = {MP3_CMD_PLAY_TRACK, 2};
                    sendMP3Command(yellowSound);
                    mode2_sounds_start_yellow();
                } else if (guildColorState == 2) {
                    Mp3Command_t redSound = {MP3_CMD_PLAY_TRACK, 2};
                    sendMP3Command(redSound);
                    mode2_sounds_start_red();
                } else if (guildColorState == 0) {
                    mode2_sounds_stop_yellow();
                    mode2_sounds_stop_red();
                }
            }
        }
    }
}

uint8_t mode2_get_current_color_state() {
    return guildColorState;
}