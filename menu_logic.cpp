/**
 * @file menu_logic.cpp
 * @brief ЛОГИКА МЕНЮ (С ДОБАВЛЕННЫМ BACK)
 * @version 3.0 (ИЗМЕНЕНА: ЗАПРЕТ ПЕРЕКЛЮЧЕНИЯ В MODE2 ПРИ ОТСУТСТВИИ ГИЛЬЗЫ)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - При попытке переключиться в MODE2 проверяется наличие датчика гильзы
 * - Если гильза отсутствует, переключение игнорируется (без сообщений)
 * - Пункт меню MODE2 остаётся видимым, но не активным для выбора
 */

#include "menu_engine.h"
#include "globals.h"
#include "system_config.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"
#include "mp3_player.h"
#include "calibration_simple.h"
#include "menu_drawing.h"

static MenuState_t currentState = MENU_STATE_MAIN;
static uint32_t lastActivityTime = 0;
static uint8_t selectedItem = 0;
static uint8_t selectedMode = 0;
static bool modeConfirmed = false;

// ============================================================================
void menu_init() {
    currentState = MENU_STATE_MAIN;
    selectedItem = 0;
    selectedMode = sysData.mode;
    modeConfirmed = false;
    lastActivityTime = millis();
    drawing_reset_cache();
    Serial.println("[MENU] Init");
}

bool menu_is_active() {
    return currentState != MENU_STATE_MAIN;
}

MenuState_t menu_get_current_state() {
    return currentState;
}

void menu_check_timeout() {
    if (currentState != MENU_STATE_MAIN) {
        uint32_t now = millis();
        if (now - lastActivityTime > MENU_INACTIVITY_TIMEOUT) {
            currentState = MENU_STATE_MAIN;
            forceDisplayRedraw = true;
        }
    }
}

// ============================================================================
void menu_handle_event(EncoderEvent_t event) {
    lastActivityTime = millis();

    switch (currentState) {

        // --------------------------------------------------------------------
        case MENU_STATE_MAIN:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_TOP;
                selectedItem = 0;
                drawMenuTop(selectedItem);
            }
            break;

        // --------------------------------------------------------------------
        case MENU_STATE_TOP:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t old = selectedItem;
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 5;
                } else {
                    selectedItem = (selectedItem == 0) ? 4 : selectedItem - 1;
                }
                updateMenuTopSelection(old, selectedItem);
            }
            else if (event == EVENT_BUTTON_CLICK) {
                switch (selectedItem) {
                    case 0: // MODE
                        currentState = MENU_STATE_MODE_SELECT;
                        selectedItem = (sysData.mode == 0) ? 2 : 1;
                        selectedMode = sysData.mode;
                        modeConfirmed = false;
                        drawMenuMode(selectedItem, selectedMode, modeConfirmed);
                        break;
                    case 1: // VOLUME
                        currentState = MENU_STATE_MP3_VOL;
                        selectedItem = 1;
                        drawing_reset_volume_cache();
                        drawMenuVolume(selectedItem, settings_get_mp3_volume());
                        break;
                    case 2: // CALIB
                        currentState = MENU_STATE_CALIB;
                        selectedItem = 0;
                        drawMenuCalib(selectedItem);
                        break;
                    case 3: // SETTINGS (заглушка)
                        // TODO: добавить позже
                        break;
                    case 4: // BACK - возврат на главный экран
                        currentState = MENU_STATE_MAIN;
                        forceDisplayRedraw = true;
                        break;
                }
            }
            break;

        // --------------------------------------------------------------------
        case MENU_STATE_MODE_SELECT:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t newItem = selectedItem;
                if (event == EVENT_ENCODER_RIGHT) {
                    newItem = (selectedItem + 1) % 4;
                } else {
                    newItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }

                // ================================================================
                // ИЗМЕНЕНИЕ: проверка доступности MODE2
                // Пункт 2 (MODE2) считается недоступным, если датчик гильзы отсутствует
                // ================================================================
                bool available = true;
                if (sysData.mode == 0 && newItem == 1) available = false;
                if (sysData.mode == 1 && newItem == 2) available = false;
                
                // ДОБАВЛЕНО: если пытаемся выбрать MODE2 (пункт 2), а гильзы нет — недоступно
                if (newItem == 2 && !sensors[3].found) available = false;

                if (!available) {
                    uint8_t secondItem;
                    if (event == EVENT_ENCODER_RIGHT) {
                        secondItem = (newItem + 1) % 4;
                    } else {
                        secondItem = (newItem == 0) ? 3 : newItem - 1;
                    }
                    bool secondAvailable = true;
                    if (sysData.mode == 0 && secondItem == 1) secondAvailable = false;
                    if (sysData.mode == 1 && secondItem == 2) secondAvailable = false;
                    if (secondItem == 2 && !sensors[3].found) secondAvailable = false;
                    
                    if (secondAvailable) {
                        newItem = secondItem;
                    } else {
                        newItem = selectedItem;
                    }
                }

                if (newItem != selectedItem) {
                    selectedItem = newItem;
                    updateMenuModeSelection(selectedItem, selectedMode, modeConfirmed);
                }
            }
            else if (event == EVENT_BUTTON_CLICK) {
                if (selectedItem == 0) {
                    currentState = MENU_STATE_TOP;
                    selectedItem = 0;
                    drawMenuTop(0);
                } else if (selectedItem == 1 || selectedItem == 2) {
                    uint8_t newMode = (selectedItem == 1) ? 0 : 1;
                    
                    // ============================================================
                    // ИЗМЕНЕНИЕ: при попытке переключиться в MODE2 проверяем наличие гильзы
                    // Если гильзы нет — переключение игнорируется (без звука, без сообщений)
                    // ============================================================
                    if (newMode == 1 && !sensors[3].found) {
                        // Молча игнорируем попытку переключения в MODE2
                        // Никаких сообщений на дисплей или в Serial
                        return;
                    }
                    
                    if (newMode != sysData.mode) {
                        selectedMode = newMode;
                        modeConfirmed = true;
                        updateMenuModeSelection(selectedItem, selectedMode, modeConfirmed);
                    }
                } else if (selectedItem == 3 && modeConfirmed) {
                    resetDisplayState(selectedMode);
                    currentState = MENU_STATE_MAIN;
                    modeConfirmed = false;
                    forceDisplayRedraw = true;
                }
            }
            break;

        // --------------------------------------------------------------------
        case MENU_STATE_MP3_VOL:
            if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
                int dir = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
                uint8_t vol = settings_get_mp3_volume() + dir;
                if (vol <= 30) {
                    settings_set_mp3_volume(vol);
                    updateMenuVolume(vol, selectedItem);
                }
            }
            else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t old = selectedItem;
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                updateMenuVolume(settings_get_mp3_volume(), selectedItem);
            }
            else if (event == EVENT_BUTTON_CLICK) {
                switch (selectedItem) {
                    case 0:
                    case 3:
                        currentState = MENU_STATE_TOP;
                        selectedItem = 1;
                        drawMenuTop(1);
                        break;
                    case 2:
                        Mp3Command_t play = { MP3_CMD_PLAY_TRACK, 1 };
                        sendMP3Command(play);
                        break;
                }
            }
            break;

        // --------------------------------------------------------------------
        case MENU_STATE_CALIB:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t old = selectedItem;
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                updateMenuCalibSelection(old, selectedItem);
            }
            else if (event == EVENT_BUTTON_CLICK) {
                switch (selectedItem) {
                    case 0: // STATUS
                        currentState = MENU_STATE_CALIB_STATUS;
                        drawCalibStatus();
                        break;
                    case 1: // RESET
                        for (int i = 0; i < 4; i++) {
                            settings_set_offset(i, 0.0f);
                        }
                        settings_save();
                        showMessage("Offsets reset", 1000);
                        currentState = MENU_STATE_CALIB;
                        drawMenuCalib(selectedItem);
                        break;
                    case 2: // AUTO
                        autoCalibrateAllSensors();
                        showMessage("Auto done", 1000);
                        currentState = MENU_STATE_CALIB;
                        drawMenuCalib(selectedItem);
                        break;
                    case 3: // BACK
                        currentState = MENU_STATE_TOP;
                        selectedItem = 2;
                        drawMenuTop(2);
                        break;
                }
            }
            break;

        // --------------------------------------------------------------------
        case MENU_STATE_CALIB_STATUS:
            if (event == EVENT_BUTTON_CLICK || event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                currentState = MENU_STATE_CALIB;
                selectedItem = 0;
                drawMenuCalib(selectedItem);
            }
            break;

        default:
            break;
    }
}