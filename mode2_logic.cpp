/**
 * ============================================================================
 * ФАЙЛ: mode2_logic.cpp
 * ЛОГИКА РАБОЧЕГО РЕЖИМА (MODE2) - УПРАВЛЕНИЕ ЦВЕТОМ И ЗВУКАМИ
 * 
 * ВЕРСИЯ: 3.0 (ИЗМЕНЕНА: НЕ РАБОТАЕТ ПРИ ОТСУТСТВИИ ДАТЧИКА ГИЛЬЗЫ)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - В mode2_update_color_state() добавлена проверка наличия датчика гильзы
 * - Если гильза отсутствует — функция ничего не делает (цвет не меняется, звуки не запускаются)
 * - При отсутствии гильзы переключение в MODE2 запрещено в menu_logic.cpp, 
 *   но эта страховка дополнительно защищает от случайных вызовов
 * ============================================================================
 */

#include "mode2_logic.h"
#include "eeprom_settings.h"
#include "globals.h"
#include "system_config.h"
#include "measurement_core.h"
#include "mp3_player.h"
#include "mode2_sounds.h"

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void mode2_set_base_temperature(float baseTemp) {
    guildBaseTemp = baseTemp;
}

float mode2_get_base_temperature() {
    return guildBaseTemp;
}

// ============================================================================
// ОБНОВЛЕНИЕ ЦВЕТОВОГО СОСТОЯНИЯ (ВЫЗЫВАЕТСЯ ИЗ ЗАДАЧИ ИЗМЕРЕНИЙ)
// ============================================================================
void mode2_update_color_state(float currentGuildTemp) {
    // ========================================================================
    // ИЗМЕНЕНИЕ: ПРОВЕРКА НАЛИЧИЯ ДАТЧИКА ГИЛЬЗЫ
    // Если датчик гильзы отсутствует — выходим, ничего не делаем.
    // Это страховка на случай, если по какой-то причине mode2_update_color_state()
    // будет вызвана при отсутствии гильзы (хотя переключение в MODE2 запрещено).
    // ========================================================================
    if (!sensors[3].found) {
        return;
    }
    
    // Проверяем что мы в MODE2 и базовая температура установлена
    if (sysData.mode == 1 && guildBaseTemp != 0.0f) {
        if (isValidTemperature(currentGuildTemp)) {
            float diff = currentGuildTemp - guildBaseTemp;
            
            // Получаем актуальные пороги из настроек
            float greenThreshold = settings_get_green_threshold();
            float yellowThreshold = settings_get_yellow_threshold();
            float hysteresis = settings_get_hysteresis();
            
            // Сохраняем предыдущее состояние для определения момента перехода
            uint8_t oldState = guildColorState;
            
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
            
            // ================================================================
            // ЗВУКОВОЕ СОПРОВОЖДЕНИЕ ПРИ СМЕНЕ ЦВЕТА
            // ================================================================
            if (oldState != guildColorState) {
                if (guildColorState == 1) {
                    // Стал ЖЁЛТЫМ (переход 0→1 или 2→1)
                    // 1. Играем предупреждающий звук
                    Mp3Command_t yellowSound = {MP3_CMD_PLAY_TRACK, 2};  // 0002.mp3
                    sendMP3Command(yellowSound);
                    Serial.println("[MODE2] 🔔 Вход в жёлтый режим: 0002.mp3");
                    
                    // 2. Запускаем цикл чередования звуков в жёлтом режиме
                    mode2_sounds_start_yellow();
                }
                else if (guildColorState == 2) {
                    // Стал КРАСНЫМ (переход 1→2)
                    Mp3Command_t redSound = {MP3_CMD_PLAY_TRACK, 2};  // 0002.mp3
                    sendMP3Command(redSound);
                    Serial.println("[MODE2] 🔔 Вход в красный режим: 0002.mp3");
                    
                    // Запускаем красный цикл
                    mode2_sounds_start_red();
                }
                else if (guildColorState == 0) {
                    // Стал ЗЕЛЁНЫМ (переход 1→0 или 2→1→0)
                    mode2_sounds_stop_yellow();   // останавливаем жёлтый (если был)
                    mode2_sounds_stop_red();      // останавливаем красный (если был)
                    Serial.println("[MODE2] 🔔 Возврат в зелёный режим (без звука)");
                }
            }
        }
    }
}

// ============================================================================
// ПОЛУЧЕНИЕ ТЕКУЩЕГО ЦВЕТОВОГО СОСТОЯНИЯ
// ============================================================================
uint8_t mode2_get_current_color_state() {
    return guildColorState;
}