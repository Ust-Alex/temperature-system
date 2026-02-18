/**
 * ============================================================================
 * ФАЙЛ: menu_engine.cpp
 * РЕАЛИЗАЦИЯ УПРАВЛЕНИЯ МЕНЮ ЭНКОДЕРА
 * 
 * ВЕРСИЯ: 5.0 (ИСПРАВЛЕННАЯ ЛОГИКА MODE)
 * 
 * ОСОБЕННОСТИ:
 * - Курсор ходит только по доступным пунктам
 * - Частичная перерисовка (только изменения)
 * - Цветовая индикация выбранного режима
 * ============================================================================
 */

#include "menu_engine.h"
#include "globals.h"
#include "system_config.h"
#include "display_engine.h"
#include "sensors.h"
#include "mp3_player.h"
#include "calibration_simple.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"

// ============================================================================
// КОНСТАНТЫ МОДУЛЯ
// ============================================================================
#define MENU_INACTIVITY_TIMEOUT 30000
#define MENU_START_Y 5
#define MENU_ITEM_HEIGHT 40
#define MENU_ITEM_WIDTH 220
#define MENU_ITEM_SPACING 5

// Цвета
#define MENU_BG_COLOR COLOR_BLACK
#define MENU_TEXT_COLOR COLOR_WHITE
#define MENU_SELECT_BG COLOR_WHITE
#define MENU_SELECT_TEXT COLOR_BLACK

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ (STATIC)
// ============================================================================
static MenuState_t currentState = MENU_STATE_MAIN;
static uint32_t lastActivityTime = 0;
static uint8_t selectedItem = 0;
static uint8_t selectedMode = 0;    // 0 = MODE1, 1 = MODE2
static bool modeConfirmed = false;  // Выбран ли режим для подтверждения

// Для частичной перерисовки
static uint8_t lastSelectedItem = 255;
static uint8_t lastVolume = 255;
static uint8_t lastSelectedMode = 255;
static uint16_t lastBgColor = 0;

// Внешние объекты
extern TFT_eSPI tft;
extern QueueHandle_t eventQueue;
extern float guildBaseTemp;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

static void clearScreen(uint16_t bgColor) {
  tft.fillScreen(bgColor);
  vTaskDelay(pdMS_TO_TICKS(10));  // Короткая задержка для стабильности
  lastSelectedItem = 255;
  lastVolume = 255;
  lastSelectedMode = 255;
  lastBgColor = bgColor;
}

// ============================================================================
// ФУНКЦИИ ОТРИСОВКИ (ПОЛНАЯ - ТОЛЬКО ПРИ ВХОДЕ)
// ============================================================================

static void drawMenuTop() {
  clearScreen(MENU_BG_COLOR);
  tft.setTextFont(FONT_DELTA);

  const char* items[] = { "MODE", "VOLUME", "CALIB", "SETTINGS" };

  for (int i = 0; i < 4; i++) {
    uint16_t bgColor = (i == selectedItem) ? MENU_SELECT_BG : MENU_BG_COLOR;
    uint16_t textColor = (i == selectedItem) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;

    tft.fillRect(10, MENU_START_Y + i * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING),
                 MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(30, MENU_START_Y + i * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING) + 10);
    tft.print(items[i]);
  }
}

static void drawMenuMode() {
  uint16_t bgColor = (sysData.mode == 0) ? COLOR_BLUE : COLOR_GREEN;
  clearScreen(bgColor);
  lastBgColor = bgColor;

  tft.setTextFont(FONT_DELTA);

  const char* items[] = { "--- MODE ---", "MODE1", "MODE2", "OK" };
  int y = MENU_START_Y;

  for (int i = 0; i < 4; i++) {
    uint16_t itemColor = bgColor;
    uint16_t textColor = MENU_TEXT_COLOR;

    // Определяем цвета в зависимости от состояния
    if (i == selectedItem) {
      itemColor = MENU_SELECT_BG;
      textColor = MENU_SELECT_TEXT;
    }

    // Подсветка выбранного режима
    if (i == 1 && selectedMode == 0 && modeConfirmed) {
      itemColor = COLOR_BLUE;
      textColor = COLOR_WHITE;
    }
    if (i == 2 && selectedMode == 1 && modeConfirmed) {
      itemColor = COLOR_GREEN;
      textColor = COLOR_WHITE;
    }

    tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, itemColor);
    tft.setTextColor(textColor, itemColor);
    tft.setCursor(40, y + 10);
    tft.print(items[i]);

    // Температуры справа
    if (i == 1) {
      tft.setCursor(150, y + 10);
      tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
    }
    if (i == 2) {
      tft.setCursor(150, y + 10);
      tft.printf("%05.2f", guildBaseTemp);
    }

    y += MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
  }
}

static void drawMenuVolume() {
  clearScreen(MENU_BG_COLOR);
  tft.setTextFont(FONT_DELTA);

  const char* items[] = { "--- VOLUME ---", "", ">>>>>", "OK" };
  int y = MENU_START_Y;

  for (int i = 0; i < 4; i++) {
    if (i != 1) {
      uint16_t bgColor = (i == selectedItem) ? MENU_SELECT_BG : MENU_BG_COLOR;
      uint16_t textColor = (i == selectedItem) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;

      tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
      tft.setTextColor(textColor, bgColor);
      tft.setCursor(40, y + 10);
      tft.print(items[i]);
    }

    if (i == 1) {
      tft.setTextFont(FONT_BIG);
      tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
      tft.setCursor(80, y + 5);
      tft.printf("%d", settings_get_mp3_volume());
      tft.setTextFont(FONT_DELTA);
    }

    y += MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
  }
}

// ============================================================================
// ФУНКЦИИ ОБНОВЛЕНИЯ (ЧАСТИЧНАЯ ПЕРЕРИСОВКА)
// ============================================================================

static void updateMenuItem(int index, uint16_t bgColor, uint16_t textColor, const char* text) {
  int y = MENU_START_Y + index * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
  tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
  tft.setTextColor(textColor, bgColor);
  tft.setCursor(40, y + 10);
  tft.print(text);
  vTaskDelay(pdMS_TO_TICKS(1));
}

static void updateMenuModeSelection() {
    // Защита индексов
    if (selectedItem > 3) selectedItem = 0;
    if (lastSelectedItem > 3 && lastSelectedItem != 255) lastSelectedItem = 0;
    if (lastSelectedItem == 255) lastSelectedItem = 0;
    
    // 1. ОБНОВЛЕНИЕ КУРСОРА (белая рамка)
    if (selectedItem != lastSelectedItem) {
        // Полностью перерисовываем старый пункт с фоном меню
        int yOld = MENU_START_Y + lastSelectedItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, lastBgColor);
        tft.setTextColor(MENU_TEXT_COLOR, lastBgColor);
        tft.setCursor(40, yOld + 10);
        
        if (lastSelectedItem == 0) tft.print("--- MODE ---");
        else if (lastSelectedItem == 1) tft.print("MODE1");
        else if (lastSelectedItem == 2) tft.print("MODE2");
        else if (lastSelectedItem == 3) tft.print("OK");
        
        // Если старый пункт - MODE1 или MODE2, перерисовываем температуру
        if (lastSelectedItem == 1) {
            tft.setCursor(150, yOld + 10);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (lastSelectedItem == 2) {
            tft.setCursor(150, yOld + 10);
            tft.printf("%05.2f", guildBaseTemp);
        }
        
        // Рисуем новый пункт с белым фоном
        int yNew = MENU_START_Y + selectedItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
        tft.setCursor(40, yNew + 10);
        
        if (selectedItem == 0) tft.print("--- MODE ---");
        else if (selectedItem == 1) tft.print("MODE1");
        else if (selectedItem == 2) tft.print("MODE2");
        else if (selectedItem == 3) tft.print("OK");
        
        // Если новый пункт - MODE1 или MODE2, рисуем температуру
        if (selectedItem == 1) {
            tft.setCursor(150, yNew + 10);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (selectedItem == 2) {
            tft.setCursor(150, yNew + 10);
            tft.printf("%05.2f", guildBaseTemp);
        }
        
        lastSelectedItem = selectedItem;
    }
    
    // 2. ОБНОВЛЕНИЕ ЦВЕТНОЙ РАМКИ (выбранный режим)
    if (modeConfirmed) {
        // Рисуем подсветку для выбранного режима (всегда, даже если курсор на OK)
        uint16_t color = (selectedMode == 0) ? COLOR_BLUE : COLOR_GREEN;
        int y = MENU_START_Y + (selectedMode + 1) * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        
        tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, color);
        tft.setTextColor(COLOR_WHITE, color);
        tft.setCursor(40, y + 10);
        tft.print(selectedMode == 0 ? "MODE1" : "MODE2");
        
        // Рисуем температуру
        tft.setCursor(150, y + 10);
        if (selectedMode == 0) {
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        } else {
            tft.printf("%05.2f", guildBaseTemp);
        }
        
        // Если курсор сейчас на этом же пункте, нужно поверх нарисовать белую рамку
        if (selectedItem == selectedMode + 1) {
            tft.drawRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        }
        
        lastSelectedMode = selectedMode;
    } else {
        // Если режим не выбран, но lastSelectedMode ещё хранит старое значение,
        // нужно стереть старую подсветку
        if (lastSelectedMode != 255) {
            int y = MENU_START_Y + (lastSelectedMode + 1) * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
            tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, lastBgColor);
            tft.setTextColor(MENU_TEXT_COLOR, lastBgColor);
            tft.setCursor(40, y + 10);
            tft.print(lastSelectedMode == 0 ? "MODE1" : "MODE2");
            
            // Рисуем температуру
            tft.setCursor(150, y + 10);
            if (lastSelectedMode == 0) {
                tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
            } else {
                tft.printf("%05.2f", guildBaseTemp);
            }
            
            lastSelectedMode = 255;
        }
    }
}



static void updateMenuVolume() {
  uint8_t vol = settings_get_mp3_volume();

  // Обновление цифры громкости
  if (vol != lastVolume) {
    int y = MENU_START_Y + MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    tft.fillRect(70, y, 100, 50, MENU_BG_COLOR);
    tft.setTextFont(FONT_BIG);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    tft.setCursor(80, y + 5);
    tft.printf("%d", vol);
    tft.setTextFont(FONT_DELTA);
    lastVolume = vol;
  }

  // Обновление выделенного пункта
  if (selectedItem != lastSelectedItem) {
    int y = MENU_START_Y;
    for (int i = 0; i < 4; i++) {
      if (i == 1) {
        y += MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
        continue;
      }

      uint16_t bgColor = (i == selectedItem) ? MENU_SELECT_BG : MENU_BG_COLOR;
      uint16_t textColor = (i == selectedItem) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;

      tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
      tft.setTextColor(textColor, bgColor);
      tft.setCursor(40, y + 10);

      if (i == 0) tft.print("--- VOLUME ---");
      else if (i == 2) tft.print(">>>>>");
      else if (i == 3) tft.print("OK");

      y += MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    }
    lastSelectedItem = selectedItem;
  }
}

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void menu_init() {
  currentState = MENU_STATE_MAIN;
  selectedItem = 0;
  selectedMode = sysData.mode;
  modeConfirmed = false;
  lastActivityTime = millis();

  lastSelectedItem = 255;
  lastVolume = 255;
  lastSelectedMode = 255;
  lastBgColor = 0;

  Serial.println("[MENU] Модуль инициализирован");
}

bool menu_is_active() {
  return currentState != MENU_STATE_MAIN;
}

void menu_check_timeout() {
  if (currentState != MENU_STATE_MAIN) {
    uint32_t now = millis();
    if (now - lastActivityTime > MENU_INACTIVITY_TIMEOUT) {
      currentState = MENU_STATE_MAIN;
      forceDisplayRedraw = true;
      Serial.println("[MENU] Таймаут - возврат в MAIN");
    }
  }
}

void menu_handle_event(EncoderEvent_t event) {
  lastActivityTime = millis();

  switch (currentState) {
    case MENU_STATE_MAIN:
      if (event == EVENT_BUTTON_CLICK) {
        currentState = MENU_STATE_TOP;
        selectedItem = 0;
        drawMenuTop();
        Serial.println("[MENU] Переход в TOP");
      }
      break;

    case MENU_STATE_TOP:
      if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t old = selectedItem;
        if (event == EVENT_ENCODER_RIGHT) {
          selectedItem = (selectedItem + 1) % 4;
        } else {
          selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }

        // Частичная перерисовка TOP
        int yOld = MENU_START_Y + old * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        int yNew = MENU_START_Y + selectedItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);

        const char* items[] = { "MODE", "VOLUME", "CALIB", "SETTINGS" };

        tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_BG_COLOR);
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
        tft.setCursor(30, yOld + 10);
        tft.print(items[old]);

        tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
        tft.setCursor(30, yNew + 10);
        tft.print(items[selectedItem]);
      } else if (event == EVENT_BUTTON_CLICK) {
        switch (selectedItem) {
          case 0:  // MODE
            currentState = MENU_STATE_MODE_SELECT;
            selectedItem = (sysData.mode == 0) ? 2 : 1;  // Сразу на доступном
            selectedMode = sysData.mode;
            modeConfirmed = false;
            drawMenuMode();
            Serial.println("[MENU] Переход в MODE");
            break;

          case 1:  // VOLUME
            currentState = MENU_STATE_MP3_VOL;
            selectedItem = 1;
            lastSelectedItem = 255;
            lastVolume = 255;
            drawMenuVolume();
            Serial.println("[MENU] Переход в VOLUME");
            break;
        }
      }
      break;

case MENU_STATE_MODE_SELECT:
    // =========================================================================
    // ОБРАБОТКА ПОВОРОТА (перемещение курсора)
    // =========================================================================
    if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t newItem = selectedItem;
        
        // Шаг 1: двигаемся в нужном направлении
        if (event == EVENT_ENCODER_RIGHT) {
            newItem = (selectedItem + 1) % 4;
        } else {
            newItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }
        
        // Шаг 2: проверяем, доступен ли новый пункт
        bool available = true;
        if (sysData.mode == 0) { // Мы в синем режиме
            if (newItem == 1) available = false; // MODE1 недоступен
        } else { // Мы в зелёном режиме
            if (newItem == 2) available = false; // MODE2 недоступен
        }
        
        // Шаг 3: если недоступен, делаем ещё шаг в том же направлении
        if (!available) {
            uint8_t secondItem = newItem;
            if (event == EVENT_ENCODER_RIGHT) {
                secondItem = (newItem + 1) % 4;
            } else {
                secondItem = (newItem == 0) ? 3 : newItem - 1;
            }
            
            // Проверяем второй вариант
            bool secondAvailable = true;
            if (sysData.mode == 0) {
                if (secondItem == 1) secondAvailable = false;
            } else {
                if (secondItem == 2) secondAvailable = false;
            }
            
            if (secondAvailable) {
                newItem = secondItem;
            } else {
                // Если и второй недоступен (маловероятно), остаёмся на месте
                newItem = selectedItem;
            }
        }
        
        // Если пункт изменился — обновляем
        if (newItem != selectedItem) {
            selectedItem = newItem;
            updateMenuModeSelection();
            Serial.printf("[MENU] MODE: пункт %d\n", selectedItem);
        }
    }
    
    // =========================================================================
    // ОБРАБОТКА НАЖАТИЯ
    // =========================================================================
    else if (event == EVENT_BUTTON_CLICK) {
        if (selectedItem == 0) {
            // Возврат в TOP
            currentState = MENU_STATE_TOP;
            selectedItem = 0;
            drawMenuTop();
            Serial.println("[MENU] Возврат в TOP");
        }
        else if (selectedItem == 1 || selectedItem == 2) {
            // Выбор режима (MODE1 или MODE2)
            uint8_t newMode = (selectedItem == 1) ? 0 : 1;
            
            // Проверяем, что это не текущий режим
            if (newMode != sysData.mode) {
                selectedMode = newMode;
                modeConfirmed = true;
                updateMenuModeSelection();
                Serial.printf("[MENU] Выбран режим %d\n", selectedMode);
            } else {
                Serial.println("[MENU] Нельзя выбрать текущий режим");
            }
        }
        else if (selectedItem == 3) {
            // OK — подтверждение выбора
            if (modeConfirmed) {
                Serial.printf("[MENU] OK: переключение в режим %d\n", selectedMode);
                resetDisplayState(selectedMode);
                currentState = MENU_STATE_MAIN;
                modeConfirmed = false;
                forceDisplayRedraw = true;
            } else {
                Serial.println("[MENU] Сначала выберите режим");
            }
        }
    }
    
    // =========================================================================
    // ОБРАБОТКА УДЕРЖАНИЯ (пока не используем)
    // =========================================================================
    else if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
        // Ничего не делаем в MODE
    }
    break;      if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t oldItem = selectedItem;
        uint8_t newItem = selectedItem;

        // Рассчитываем новый пункт с учётом доступности
        if (event == EVENT_ENCODER_RIGHT) {
          newItem = (selectedItem + 1) % 4;
        } else {
          newItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }

        // Проверяем доступность пункта
        bool available = true;
        if (sysData.mode == 0) {                // В синем режиме
          if (newItem == 1) available = false;  // MODE1 недоступен
        } else {                                // В зелёном режиме
          if (newItem == 2) available = false;  // MODE2 недоступен
        }

        // Если пункт доступен - переходим, иначе пробуем следующий
        if (available) {
          selectedItem = newItem;
        } else {
          // Пропускаем недоступный пункт
          if (event == EVENT_ENCODER_RIGHT) {
            selectedItem = (newItem + 1) % 4;
          } else {
            selectedItem = (newItem == 0) ? 3 : newItem - 1;
          }
        }

        updateMenuModeSelection();
        Serial.printf("[MENU] MODE: пункт %d\n", selectedItem);
      } else if (event == EVENT_BUTTON_CLICK) {
        if (selectedItem == 0) {
          // Возврат в TOP
          currentState = MENU_STATE_TOP;
          selectedItem = 0;
          drawMenuTop();
          Serial.println("[MENU] Возврат в TOP");
        } else if ((selectedItem == 1 && sysData.mode != 0) || (selectedItem == 2 && sysData.mode != 1)) {
          // Выбор противоположного режима
          selectedMode = (selectedItem == 1) ? 0 : 1;
          modeConfirmed = true;
          updateMenuModeSelection();
          Serial.printf("[MENU] Выбран режим %d\n", selectedMode);
        } else if (selectedItem == 3 && modeConfirmed) {
          // OK - подтверждение
          Serial.printf("[MENU] Подтверждение: переход в режим %d\n", selectedMode);
          resetDisplayState(selectedMode);
          currentState = MENU_STATE_MAIN;
          modeConfirmed = false;
          forceDisplayRedraw = true;
        }
      }
      break;

    case MENU_STATE_MP3_VOL:
      if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
        int dir = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
        uint8_t vol = settings_get_mp3_volume() + dir;
        if (vol <= 30) {
          settings_set_mp3_volume(vol);
          updateMenuVolume();
        }
      } else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
        uint8_t old = selectedItem;
        if (event == EVENT_ENCODER_RIGHT) {
          selectedItem = (selectedItem + 1) % 4;
        } else {
          selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
        }
        updateMenuVolume();
      } else if (event == EVENT_BUTTON_CLICK) {
        if (selectedItem == 0) {
          currentState = MENU_STATE_TOP;
          selectedItem = 1;
          drawMenuTop();
        } else if (selectedItem == 2) {
          Mp3Command_t play = { MP3_CMD_PLAY_TRACK, 1 };
          sendMP3Command(play);
        } else if (selectedItem == 3) {
          currentState = MENU_STATE_TOP;
          selectedItem = 1;
          drawMenuTop();
        }
      }
      break;

    default:
      break;
  }
}