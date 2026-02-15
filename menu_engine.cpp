/**
 * ============================================================================
 * ФАЙЛ: menu_engine.cpp
 * РЕАЛИЗАЦИЯ УПРАВЛЕНИЯ МЕНЮ ЭНКОДЕРА
 * 
 * ВЕРСИЯ: 1.2 (ИСПРАВЛЕНЫ ЦВЕТА ФОНА И ОЧИСТКА ЭКРАНА)
 * 
 * ОСОБЕННОСТИ:
 * - При входе в MODE_SELECT фон соответствует текущему режиму
 * - Полная очистка экрана при входе в меню (никаких "проступающих" цифр)
 * - Единый шрифт FONT_DELTA для всего текста
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
#define MENU_ITEM_HEIGHT 40             // Высота одного пункта меню
#define MENU_ITEM_WIDTH  220            // Ширина пункта меню
#define MENU_START_Y     100            // Начальная Y-координата

// Цвета текста в меню
#define MENU_TEXT_COLOR   COLOR_WHITE
#define MENU_SELECT_BG    COLOR_WHITE
#define MENU_SELECT_TEXT  COLOR_BLACK
#define MENU_HIGHLIGHT    COLOR_CYAN    // Для подсветки при удержании

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ (STATIC)
// ============================================================================
static MenuState_t currentState = MENU_STATE_MAIN;
static uint32_t lastActivityTime = 0;
static uint8_t selectedItem = 0;         // Выбранный пункт в текущем подменю
static bool holdMode = false;             // Режим удержания (для изменения параметров)

// Внешние объекты
extern TFT_eSPI tft;
extern QueueHandle_t eventQueue;
extern uint8_t guildColorState;
extern float guildBaseTemp;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (ОТРИСОВКА)
// ============================================================================

/**
 * Полная очистка экрана с заливкой цветом фона
 * Используется при входе в любое меню для предотвращения "проступания" старых цифр
 */
static void clearScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    vTaskDelay(pdMS_TO_TICKS(10)); // Короткая задержка для гарантии записи в дисплей
}

/**
 * Отрисовка меню выбора режима (MODE_SELECT)
 * Фон соответствует текущему режиму: синий для MODE1, зелёный для MODE2
 */
static void drawMenuModeSelect() {
    // Определяем цвет фона в зависимости от текущего режима
    uint16_t bgColor = (sysData.mode == 0) ? COLOR_BLUE : COLOR_GREEN;
    
    // Полная очистка экрана с заливкой цветом фона
    clearScreen(bgColor);
    
    float currentTemp = sensors[3].found ? sensors[3].temp : 0.0f;
    float baseTemp = guildBaseTemp;
    
    // Устанавливаем единый шрифт
    tft.setTextFont(FONT_DELTA);
    
    // Пункт MODE1
    uint16_t bgColor1 = (selectedItem == 0) ? MENU_SELECT_BG : bgColor;
    uint16_t textColor1 = (selectedItem == 0) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor1);
    tft.setTextColor(textColor1, bgColor1);
    tft.setCursor(20, MENU_START_Y + 10);
    tft.print("MODE1");
    tft.setCursor(150, MENU_START_Y + 10);
    tft.printf("%.2f C", currentTemp);
    
    // Пункт MODE2
    uint16_t bgColor2 = (selectedItem == 1) ? MENU_SELECT_BG : bgColor;
    uint16_t textColor2 = (selectedItem == 1) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + MENU_ITEM_HEIGHT + 10, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor2);
    tft.setTextColor(textColor2, bgColor2);
    tft.setCursor(20, MENU_START_Y + MENU_ITEM_HEIGHT + 20);
    tft.print("MODE2");
    tft.setCursor(150, MENU_START_Y + MENU_ITEM_HEIGHT + 20);
    tft.printf("%.2f C", baseTemp);
    
    // Подсказка внизу
    tft.setTextColor(MENU_TEXT_COLOR, bgColor);
    tft.setCursor(10, 280);
    tft.print("Click: next  Hold+turn: preview");
}

/**
 * Отрисовка меню регулировки громкости MP3
 */
static void drawMenuMp3Vol() {
    // Чёрный фон для этого меню
    clearScreen(COLOR_BLACK);
    
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("MP3 Volume");
    
    // Полоска громкости
    uint8_t vol = 15; // TODO: получить реальную громкость
    int barWidth = map(vol, 0, 30, 0, 200);
    
    tft.fillRect(20, 120, 200, 20, TFT_DARKGREY);
    tft.fillRect(20, 120, barWidth, 20, COLOR_CYAN);
    
    tft.setCursor(20, 150);
    tft.printf("%d/30", vol);
    
    tft.setCursor(10, 280);
    tft.print("Hold+turn: change  Click: next");
}

/**
 * Отрисовка меню калибровки
 */
static void drawMenuCalib() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("Calibration");
    
    for (int i = 0; i < 4; i++) {
        tft.setCursor(20, 120 + i * 30);
        if (i == selectedItem) {
            tft.setTextColor(COLOR_BLACK, COLOR_CYAN);
            tft.printf("> Sensor %d: %+.2f C", i, calibrationOffsets[i]);
            tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
        } else {
            tft.printf("  Sensor %d: %+.2f C", i, calibrationOffsets[i]);
        }
    }
    
    tft.setCursor(10, 280);
    tft.print("Turn: select  Hold+turn: adjust");
}

/**
 * Отрисовка меню статуса
 */
static void drawMenuStatus() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("System Status");
    tft.setCursor(20, 120);
    tft.printf("Mode: %s", sysData.mode == 0 ? "MODE1" : "MODE2");
    tft.setCursor(20, 150);
    tft.printf("Guild: %s", sensors[3].found ? "OK" : "LOST");
    tft.setCursor(20, 180);
    tft.printf("Error: %s", criticalError ? "YES" : "NO");
    tft.setCursor(20, 210);
    tft.printf("Uptime: %lu min", millis() / 60000);
    tft.setCursor(20, 240);
    tft.printf("Base: %.2f C", guildBaseTemp);
    
    tft.setCursor(10, 280);
    tft.print("Click: next");
}

/**
 * Отрисовка меню настроек
 */
static void drawMenuSettings() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("Settings");
    
    const char* items[] = {"Green/Yellow", "Yellow/Red", "Hysteresis"};
    float values[] = {
        settings_get_green_threshold(),
        settings_get_yellow_threshold(),
        settings_get_hysteresis()
    };
    
    for (int i = 0; i < 3; i++) {
        tft.setCursor(20, 120 + i * 30);
        if (i == selectedItem) {
            tft.setTextColor(COLOR_BLACK, COLOR_CYAN);
            tft.printf("> %s: %.3f", items[i], values[i]);
            tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
        } else {
            tft.printf("  %s: %.3f", items[i], values[i]);
        }
    }
    
    tft.setCursor(10, 280);
    tft.print("Turn: select  Hold+turn: change");
}

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void menu_init() {
    currentState = MENU_STATE_MAIN;
    selectedItem = 0;
    holdMode = false;
    lastActivityTime = millis();
    Serial.println("[MENU] Модуль инициализирован");
}

MenuState_t menu_get_current_state() {
    return currentState;
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
    
    // ========================================================================
    // РЕЖИМ УДЕРЖАНИЯ (ИЗМЕНЕНИЕ ПАРАМЕТРОВ)
    // ========================================================================
    if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
        int direction = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
        
        switch (currentState) {
            case MENU_STATE_MODE_SELECT:
                // Предварительный просмотр цвета фона
                if (selectedItem == 0) { // MODE1 выбран
                    tft.fillScreen(COLOR_BLUE);
                } else { // MODE2 выбран
                    tft.fillScreen(COLOR_GREEN);
                }
                break;
                
            case MENU_STATE_MP3_VOL:
                // Изменение громкости
                // TODO: реализовать
                break;
                
            case MENU_STATE_CALIB:
                // Изменение калибровки
                if (selectedItem >= 0 && selectedItem < 4) {
                    calibrationOffsets[selectedItem] += 0.05f * direction;
                    drawMenuCalib();
                }
                break;
                
            case MENU_STATE_SETTINGS:
                // Изменение настроек
                if (selectedItem == 0) {
                    float val = settings_get_green_threshold() + 0.01f * direction;
                    if (val > 0 && val < 1.0) settings_set_green_threshold(val);
                } else if (selectedItem == 1) {
                    float val = settings_get_yellow_threshold() + 0.01f * direction;
                    if (val > 0 && val < 1.0) settings_set_yellow_threshold(val);
                } else if (selectedItem == 2) {
                    float val = settings_get_hysteresis() + 0.005f * direction;
                    if (val > 0 && val < 0.1) settings_set_hysteresis(val);
                }
                drawMenuSettings();
                break;
                
            default:
                break;
        }
        return;
    }
    
    // ========================================================================
    // ОБЫЧНЫЙ РЕЖИМ (БЕЗ УДЕРЖАНИЯ)
    // ========================================================================
    switch (currentState) {
        case MENU_STATE_MAIN:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_MODE_SELECT;
                selectedItem = (sysData.mode == 0) ? 0 : 1;
                drawMenuModeSelect();
                Serial.println("[MENU] Переход в MODE_SELECT");
            }
            break;
            
        case MENU_STATE_MODE_SELECT:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_MP3_VOL;
                drawMenuMp3Vol();
                Serial.println("[MENU] Переход в MP3_VOL");
            }
            else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 2;
                } else {
                    selectedItem = (selectedItem == 0) ? 1 : 0;
                }
                drawMenuModeSelect();
            }
            break;
            
        case MENU_STATE_MP3_VOL:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_CALIB;
                selectedItem = 0;
                drawMenuCalib();
                Serial.println("[MENU] Переход в CALIB");
            }
            break;
            
        case MENU_STATE_CALIB:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_STATUS;
                drawMenuStatus();
                Serial.println("[MENU] Переход в STATUS");
            }
            else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                drawMenuCalib();
            }
            break;
            
        case MENU_STATE_STATUS:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_SETTINGS;
                selectedItem = 0;
                drawMenuSettings();
                Serial.println("[MENU] Переход в SETTINGS");
            }
            break;
            
        case MENU_STATE_SETTINGS:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_WIFI;
                // drawMenuWifi(); // TODO
                Serial.println("[MENU] Переход в WIFI");
            }
            else if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 3;
                } else {
                    selectedItem = (selectedItem == 0) ? 2 : selectedItem - 1;
                }
                drawMenuSettings();
            }
            break;
            
        case MENU_STATE_WIFI:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_MAIN;
                forceDisplayRedraw = true;
                Serial.println("[MENU] Возврат в MAIN");
            }
            break;
            
        default:
            break;
    }
}