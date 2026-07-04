/**
 * @file menu_logic.cpp
 * @brief ЛОГИКА МЕНЮ (С ДОБАВЛЕННЫМ WI-FI СТАТУСОМ И СБРОСОМ)
 * @version 3.4 (ИСПРАВЛЕНА РЕГУЛИРОВКА ГРОМКОСТИ)
 * 
 * ИНДЕКСЫ ГЛАВНОГО МЕНЮ:
 * 0 - MODE
 * 1 - WIFI
 * 2 - CALIB
 * 3 - SETTINGS
 * 4 - VOLUME
 * 5 - BACK
 */

#include "menu_engine.h"
#include "globals.h"
#include "system_config.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"
#include "mp3_player.h"
#include "calibration_simple.h"
#include "menu_drawing.h"
#include "wifi_config.h"
#include "wifi_manager.h"
#include "wifi_utils.h"
#include "web_server.h"

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
          selectedItem = (selectedItem + 1) % 6;
        } else {
          selectedItem = (selectedItem == 0) ? 5 : selectedItem - 1;
        }
        updateMenuTopSelection(old, selectedItem);
      } else if (event == EVENT_BUTTON_CLICK) {
        switch (selectedItem) {
          case 0:  // MODE
            currentState = MENU_STATE_MODE_SELECT;
            selectedItem = (sysData.mode == 0) ? 2 : 1;
            selectedMode = sysData.mode;
            modeConfirmed = false;
            drawMenuMode(selectedItem, selectedMode, modeConfirmed);
            break;
          case 1:  // WIFI (НОВОЕ: показываем статус)
            currentState = MENU_STATE_WIFI;
            drawWiFiStatus();
            break;
          case 2:  // CALIB
            currentState = MENU_STATE_CALIB;
            selectedItem = 0;
            drawMenuCalib(selectedItem);
            break;
          case 3:  // SETTINGS
            // TODO: добавить позже
            break;
          case 4:  // VOLUME
            currentState = MENU_STATE_MP3_VOL;
            selectedItem = 1;
            drawing_reset_volume_cache();
            drawMenuVolume(selectedItem, settings_get_mp3_volume());
            break;
          case 5:  // BACK
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

        bool available = true;
        if (sysData.mode == 0 && newItem == 1) available = false;
        if (sysData.mode == 1 && newItem == 2) available = false;
        if (newItem == 2 && !sensors[4].found) available = false;

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
          if (secondItem == 2 && !sensors[4].found) secondAvailable = false;

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
      } else if (event == EVENT_BUTTON_CLICK) {
        if (selectedItem == 0) {
          currentState = MENU_STATE_TOP;
          selectedItem = 0;
          drawMenuTop(0);
        } else if (selectedItem == 1 || selectedItem == 2) {
          uint8_t newMode = (selectedItem == 1) ? 0 : 1;

          if (newMode == 1 && !sensors[4].found) {
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
          settings_save();
          Mp3Command_t volCmd = { MP3_CMD_SET_VOLUME, vol };
          sendMP3Command(volCmd);
          updateMenuVolume(vol, selectedItem);
        }
      } else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t old = selectedItem;
        if (event == EVENT_ENCODER_RIGHT) {
          selectedItem = (selectedItem + 1) % 4;
        } else {
          selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }
        updateMenuVolume(settings_get_mp3_volume(), selectedItem);
      } else if (event == EVENT_BUTTON_CLICK) {
        switch (selectedItem) {
          case 0:
          case 3:
            currentState = MENU_STATE_TOP;
            selectedItem = 4;
            drawMenuTop(4);
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
      } else if (event == EVENT_BUTTON_CLICK) {
        switch (selectedItem) {
          case 0:  // STATUS
            currentState = MENU_STATE_CALIB_STATUS;
            drawCalibStatus();
            break;
          case 1:  // RESET
            for (int i = 0; i < 6; i++) {
              settings_set_offset(i, 0.0f);
            }
            settings_save();
            showMessage("Offsets reset", 1000);
            currentState = MENU_STATE_CALIB;
            drawMenuCalib(selectedItem);
            break;
          case 2:  // AUTO
            autoCalibrateAllSensors();
            showMessage("Auto done", 1000);
            currentState = MENU_STATE_CALIB;
            drawMenuCalib(selectedItem);
            break;
          case 3:  // BACK
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

    // --------------------------------------------------------------------
    // НОВЫЙ ЭКРАН: WI-FI СТАТУС
    // --------------------------------------------------------------------
    case MENU_STATE_WIFI:
      // Удержание вправо → сброс настроек Wi-Fi и перезагрузка в AP
      if (event == EVENT_HOLD_RIGHT) {
        Serial.println("[MENU] Сброс настроек Wi-Fi, переключение в AP...");
        settings_clear_wifi();
        showMessage("WiFi reset to AP", 1500);
        delay(1500);
        ESP.restart();
      }
      // Любое другое действие (клик или поворот) → возврат в главное меню
      else if (event == EVENT_BUTTON_CLICK || event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        currentState = MENU_STATE_TOP;
        selectedItem = 1;  // подсветка на WIFI
        drawMenuTop(selectedItem);
      }
      break;

    // --------------------------------------------------------------------
    // MENU_STATE_SETTINGS (пока заглушка)
    // --------------------------------------------------------------------
    case MENU_STATE_SETTINGS:
      if (event == EVENT_BUTTON_CLICK || event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        currentState = MENU_STATE_TOP;
        selectedItem = 3;
        drawMenuTop(selectedItem);
      }
      break;

    default:
      break;
  }
}