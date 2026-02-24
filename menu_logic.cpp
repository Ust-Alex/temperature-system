/**
 * @file menu_logic.cpp
 * @brief ЛОГИКА МЕНЮ (ПОЛНАЯ ВЕРСИЯ)
 * 
 * @version 2.0
 * @details Обрабатывает события энкодера и управляет состоянием меню.
 *          Все функции отрисовки вынесены в menu_drawing.cpp
 */

#include "menu_engine.h"
#include "globals.h"
#include "system_config.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"
#include "mp3_player.h"
#include "menu_drawing.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
// ============================================================================
static MenuState_t currentState = MENU_STATE_MAIN;
static uint32_t lastActivityTime = 0;
static uint8_t selectedItem = 0;
static uint8_t selectedMode = 0;  // 0 = MODE1, 1 = MODE2
static bool modeConfirmed = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void menu_init() {
  currentState = MENU_STATE_MAIN;
  selectedItem = 0;
  selectedMode = sysData.mode;
  modeConfirmed = false;
  lastActivityTime = millis();

  drawing_reset_cache();

  Serial.println("[MENU LOGIC] Модуль инициализирован");
}

// ============================================================================
// ПОЛУЧЕНИЕ СОСТОЯНИЯ
// ============================================================================
bool menu_is_active() {
  return currentState != MENU_STATE_MAIN;
}

MenuState_t menu_get_current_state() {
  return currentState;
}

// ============================================================================
// ПРОВЕРКА ТАЙМАУТА
// ============================================================================
void menu_check_timeout() {
  if (currentState != MENU_STATE_MAIN) {
    uint32_t now = millis();
    if (now - lastActivityTime > MENU_INACTIVITY_TIMEOUT) {
      currentState = MENU_STATE_MAIN;
      forceDisplayRedraw = true;
      Serial.println("[MENU LOGIC] Таймаут - возврат в MAIN");
    }
  }
}

// ============================================================================
// ОБРАБОТКА СОБЫТИЙ ЭНКОДЕРА
// ============================================================================
void menu_handle_event(EncoderEvent_t event) {
  // Любое событие сбрасывает таймер неактивности
  lastActivityTime = millis();

  Serial.print("[MENU EVENT] ");
  switch (event) {
    case EVENT_HOLD_LEFT: Serial.println("HOLD_LEFT"); break;
    case EVENT_HOLD_RIGHT: Serial.println("HOLD_RIGHT"); break;
    case EVENT_ENCODER_LEFT: Serial.println("LEFT"); break;
    case EVENT_ENCODER_RIGHT: Serial.println("RIGHT"); break;
    case EVENT_BUTTON_CLICK: Serial.println("CLICK"); break;
    default: Serial.println("OTHER"); break;
  }


  switch (currentState) {

    // --------------------------------------------------------------------
    // ГЛАВНЫЙ ЭКРАН
    // --------------------------------------------------------------------
    case MENU_STATE_MAIN:
      if (event == EVENT_BUTTON_CLICK) {
        currentState = MENU_STATE_TOP;
        selectedItem = 0;
        drawMenuTop(selectedItem);
        Serial.println("[MENU LOGIC] Переход в TOP");
      }
      break;

    // --------------------------------------------------------------------
    // ВЕРХНЕЕ МЕНЮ (MODE, VOLUME, CALIB, SETTINGS)
    // --------------------------------------------------------------------
    case MENU_STATE_TOP:
      if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t oldItem = selectedItem;

        if (event == EVENT_ENCODER_RIGHT) {
          selectedItem = (selectedItem + 1) % 4;
        } else {
          selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }

        updateMenuTopSelection(oldItem, selectedItem);
      } else if (event == EVENT_BUTTON_CLICK) {
        switch (selectedItem) {
          case 0:  // MODE
            currentState = MENU_STATE_MODE_SELECT;
            selectedItem = (sysData.mode == 0) ? 2 : 1;
            selectedMode = sysData.mode;
            modeConfirmed = false;
            drawMenuMode(selectedItem, selectedMode, modeConfirmed);
            Serial.println("[MENU LOGIC] Переход в MODE");
            break;

          case 1:  // VOLUME
            currentState = MENU_STATE_MP3_VOL;
            selectedItem = 1;  // Курсор на цифре
            drawing_reset_volume_cache();
            drawMenuVolume(selectedItem, settings_get_mp3_volume());
            Serial.println("[MENU LOGIC] Переход в VOLUME");
            break;

            // case 2: CALIB (будет позже)
            // case 3: SETTINGS (будет позже)
        }
      }
      break;

    // --------------------------------------------------------------------
    // ЭКРАН ВЫБОРА РЕЖИМА
    // --------------------------------------------------------------------
    case MENU_STATE_MODE_SELECT:
      if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t newItem = selectedItem;

        // Шаг 1: двигаемся в нужном направлении
        if (event == EVENT_ENCODER_RIGHT) {
          newItem = (selectedItem + 1) % 4;
        } else {
          newItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }

        // Шаг 2: проверяем доступность
        bool available = true;
        if (sysData.mode == 0) {                // Синий режим
          if (newItem == 1) available = false;  // MODE1 недоступен
        } else {                                // Зелёный режим
          if (newItem == 2) available = false;  // MODE2 недоступен
        }

        // Шаг 3: если недоступен, делаем ещё шаг
        if (!available) {
          uint8_t secondItem;
          if (event == EVENT_ENCODER_RIGHT) {
            secondItem = (newItem + 1) % 4;
          } else {
            secondItem = (newItem == 0) ? 3 : newItem - 1;
          }

          bool secondAvailable = true;
          if (sysData.mode == 0) {
            if (secondItem == 1) secondAvailable = false;
          } else {
            if (secondItem == 2) secondAvailable = false;
          }

          if (secondAvailable) {
            newItem = secondItem;
          } else {
            newItem = selectedItem;  // Остаёмся на месте
          }
        }

        // Если пункт изменился — обновляем
        if (newItem != selectedItem) {
          selectedItem = newItem;
          updateMenuModeSelection(selectedItem, selectedMode, modeConfirmed);
        }
      } else if (event == EVENT_BUTTON_CLICK) {
        if (selectedItem == 0) {
          // Возврат в TOP по заголовку
          currentState = MENU_STATE_TOP;
          selectedItem = 0;
          drawMenuTop(0);
          Serial.println("[MENU LOGIC] Возврат в TOP");
        } else if (selectedItem == 1 || selectedItem == 2) {
          // Выбор режима
          uint8_t newMode = (selectedItem == 1) ? 0 : 1;

          if (newMode != sysData.mode) {
            selectedMode = newMode;
            modeConfirmed = true;
            updateMenuModeSelection(selectedItem, selectedMode, modeConfirmed);
            Serial.printf("[MENU LOGIC] Выбран режим %d\n", selectedMode);
          }
        } else if (selectedItem == 3 && modeConfirmed) {
          // OK - подтверждение
          Serial.printf("[MENU LOGIC] OK: переход в режим %d\n", selectedMode);
          resetDisplayState(selectedMode);
          currentState = MENU_STATE_MAIN;
          modeConfirmed = false;
          forceDisplayRedraw = true;
        }
      }
      break;

    // --------------------------------------------------------------------
    // ЭКРАН РЕГУЛИРОВКИ ГРОМКОСТИ
    // --------------------------------------------------------------------
    case MENU_STATE_MP3_VOL:
      if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
        // Удержание меняет громкость (работает всегда)
        int dir = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
        uint8_t vol = settings_get_mp3_volume() + dir;
        if (vol <= 30) {
          settings_set_mp3_volume(vol);
          updateMenuVolume(vol, selectedItem);
        }
      } else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        // Вращение переключает пункты меню
        uint8_t oldItem = selectedItem;

        if (event == EVENT_ENCODER_RIGHT) {
          selectedItem = (selectedItem + 1) % 4;
        } else {
          selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }

        updateMenuVolume(settings_get_mp3_volume(), selectedItem);
      } else if (event == EVENT_BUTTON_CLICK) {
        switch (selectedItem) {
          case 0:  // Заголовок - возврат
            currentState = MENU_STATE_TOP;
            selectedItem = 1;
            drawMenuTop(1);
            Serial.println("[MENU LOGIC] Возврат в TOP");
            break;

          case 2:  // >>>>> - тестовый звук
            {
              Mp3Command_t play = { MP3_CMD_PLAY_TRACK, 1 };
              sendMP3Command(play);
              Serial.println("[MENU LOGIC] Тестовый звук");
            }
            break;

          case 3:  // ok - возврат
            currentState = MENU_STATE_TOP;
            selectedItem = 1;
            drawMenuTop(1);
            break;

            // case 1 (цифра) - ничего не делаем
        }
      }
      break;

    // --------------------------------------------------------------------
    // НЕРЕАЛИЗОВАННЫЕ СОСТОЯНИЯ
    // --------------------------------------------------------------------
    default:
      break;
  }
}