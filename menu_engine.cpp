/**
 * ============================================================================
 * ФАЙЛ: menu_engine.cpp
 * РЕАЛИЗАЦИЯ УПРАВЛЕНИЯ МЕНЮ ЭНКОДЕРА
 * 
 * ВЕРСИЯ: 4.0 (МИНИМАЛИЗМ + ЧАСТИЧНАЯ ПЕРЕРИСОВКА)
 * 
 * ОСОБЕННОСТИ:
 * - Только самое необходимое на экранах
 * - Частичная перерисовка (без мерцания)
 * - Единая структура для всех меню
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
#define MENU_INACTIVITY_TIMEOUT 30000  // 30 секунд до автовозврата
#define MENU_START_Y     5   // Отступ первого пункта от верхнего края
#define MENU_ITEM_HEIGHT 40   // Высота одного пункта меню
#define MENU_ITEM_WIDTH  220  // Ширина пункта (240 - 20 = 220, отступы по бокам 10)
#define MENU_ITEM_SPACING 5  // Расстояние между пунктами

// Цвета
#define MENU_BG_COLOR     COLOR_BLACK
#define MENU_TEXT_COLOR   COLOR_WHITE
#define MENU_SELECT_BG    COLOR_WHITE
#define MENU_SELECT_TEXT  COLOR_BLACK

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ (STATIC)
// ============================================================================
static MenuState_t currentState = MENU_STATE_MAIN;
static uint32_t lastActivityTime = 0;
static uint8_t selectedItem = 0;
static uint8_t selectedMode = 0;
static bool modeConfirmed = false;

// Для частичной перерисовки
static uint8_t lastSelectedItem = 255;
static uint8_t lastVolume = 255;
static uint8_t lastMode = 255;
static float lastTemp[4] = { -1000, -1000, -1000, -1000 };

// Внешние объекты
extern TFT_eSPI tft;
extern QueueHandle_t eventQueue;
extern float guildBaseTemp;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

static void clearScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    // Сбрасываем кэш при полной очистке
    lastSelectedItem = 255;
    lastVolume = 255;
    lastMode = 255;
    for (int i = 0; i < 4; i++) lastTemp[i] = -1000;
}

// ============================================================================
// ФУНКЦИИ ОТРИСОВКИ (ПОЛНАЯ - ТОЛЬКО ПРИ ВХОДЕ)
// ============================================================================

static void drawMenuTop() {
    clearScreen(MENU_BG_COLOR);
    tft.setTextFont(FONT_DELTA);
    
    const char* items[] = {"MODE", "VOLUME", "CALIB", "SETTINGS"};
    
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
    tft.setTextFont(FONT_DELTA);
    
    const char* items[] = {"--- MODE ---", "MODE1", "MODE2", "OK"};
    int y = MENU_START_Y;
    
    for (int i = 0; i < 4; i++) {
        uint16_t itemColor = bgColor;
        uint16_t textColor = MENU_TEXT_COLOR;
        
        if (i == selectedItem) {
            itemColor = MENU_SELECT_BG;
            textColor = MENU_SELECT_TEXT;
        } else if (i == 1 && selectedMode == 0) {
            itemColor = COLOR_BLUE;
            textColor = COLOR_WHITE;
        } else if (i == 2 && selectedMode == 1) {
            itemColor = COLOR_GREEN;
            textColor = COLOR_WHITE;
        }
        
        tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, itemColor);
        tft.setTextColor(textColor, itemColor);
        tft.setCursor(40, y + 10);
        tft.print(items[i]);
        
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
    
    const char* items[] = {"--- VOLUME ---", "", ">>>>>", "OK"};
    int y = MENU_START_Y;
    
    for (int i = 0; i < 4; i++) {
        uint16_t bgColor = (i == selectedItem) ? MENU_SELECT_BG : MENU_BG_COLOR;
        uint16_t textColor = (i == selectedItem) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
        
        if (i != 1) {  // Для всех пунктов кроме цифры
            tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
            tft.setTextColor(textColor, bgColor);
            tft.setCursor(40, y + 10);
            tft.print(items[i]);
        }
        
        if (i == 1) {  // Цифра громкости
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
            if (i == 1) {  // Пропускаем цифру
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
                if (event == EVENT_ENCODER_RIGHT) selectedItem = (selectedItem + 1) % 4;
                else selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                
                // Частичная перерисовка TOP (только старый и новый пункты)
                int yOld = MENU_START_Y + old * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
                int yNew = MENU_START_Y + selectedItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
                
                tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_BG_COLOR);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                tft.setCursor(30, yOld + 10);
                const char* items[] = {"MODE", "VOLUME", "CALIB", "SETTINGS"};
                tft.print(items[old]);
                
                tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                tft.setCursor(30, yNew + 10);
                tft.print(items[selectedItem]);
            }
            else if (event == EVENT_BUTTON_CLICK) {
                switch (selectedItem) {
                    case 0: // MODE
                        currentState = MENU_STATE_MODE_SELECT;
                        selectedItem = 1;
                        selectedMode = sysData.mode;
                        modeConfirmed = false;
                        drawMenuMode();
                        break;
                    case 1: // VOLUME
                        currentState = MENU_STATE_MP3_VOL;
                        selectedItem = 1;
                        lastSelectedItem = 255;
                        lastVolume = 255;
                        drawMenuVolume();
                        break;
                    default:
                        currentState = MENU_STATE_TOP;
                        drawMenuTop();
                        break;
                }
                Serial.printf("[MENU] Переход в %d\n", selectedItem);
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
            }
            else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t old = selectedItem;
                if (event == EVENT_ENCODER_RIGHT) selectedItem = (selectedItem + 1) % 4;
                else selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                updateMenuVolume();
            }
            else if (event == EVENT_BUTTON_CLICK) {
                if (selectedItem == 0) { // Выход
                    currentState = MENU_STATE_TOP;
                    selectedItem = 1;
                    drawMenuTop();
                }
                else if (selectedItem == 2) { // Play
                    Mp3Command_t play = {MP3_CMD_PLAY_TRACK, 1};
                    sendMP3Command(play);
                }
                else if (selectedItem == 3) { // OK
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