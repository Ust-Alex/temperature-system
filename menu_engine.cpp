/**
 * ============================================================================
 * ФАЙЛ: menu_engine.cpp
 * РЕАЛИЗАЦИЯ УПРАВЛЕНИЯ МЕНЮ ЭНКОДЕРА
 * 
 * ВЕРСИЯ: 2.0 (НОВАЯ ЛОГИКА ПОДТВЕРЖДЕНИЯ ЧЕРЕЗ OK)
 * 
 * СТРУКТУРА МЕНЮ:
 * - MAIN (главный экран)
 * - TOP (верхнее меню: MODE, VOLUME, CALIB, SETTINGS)
 * - MODE (выбор режима с подтверждением через OK)
 * - VOLUME (регулировка громкости)
 * - CALIB (калибровка датчиков)
 * - SETTINGS (настройки порогов)
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
#define MENU_START_Y     60             // Начальная Y-координата (сдвинул, чтобы влезло 4 пункта)

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
static uint8_t selectedMode = 0;          // 0 = MODE1, 1 = MODE2 (для подтверждения)

// Внешние объекты
extern TFT_eSPI tft;
extern QueueHandle_t eventQueue;
extern uint8_t guildColorState;
extern float guildBaseTemp;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

/**
 * Полная очистка экрана с гарантией, что старые цифры исчезнут
 */
static void clearScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    // Явно затираем области, где в MAIN были цифры
    for (int i = 0; i < 4; i++) {
        int y = displayYPositions[i];
        tft.fillRect(10, y, maxTempWidth, bigFontHeight, bgColor);
        tft.fillRect(170, y, 70, 30, bgColor);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
}

/**
 * Отрисовка верхнего меню (TOP)
 */
static void drawMenuTop() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    
    const char* items[] = {"MODE", "VOLUME", "CALIB", "SETTINGS"};
    
    for (int i = 0; i < 4; i++) {
        uint16_t bgColor = (selectedItem == i) ? MENU_SELECT_BG : COLOR_BLACK;
        uint16_t textColor = (selectedItem == i) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
        
        tft.fillRect(10, MENU_START_Y + i * (MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
        tft.setTextColor(textColor, bgColor);
        tft.setCursor(30, MENU_START_Y + i * (MENU_ITEM_HEIGHT + 10) + 10);
        tft.print(items[i]);
    }
    
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    tft.setCursor(10, 280);
    tft.print("Select: turn  Choose: click");
}

/**
 * Отрисовка меню выбора режима (MODE)
 * Пункты: 0 = "--- MODE ---" (возврат), 1 = MODE1, 2 = MODE2, 3 = OK
 */
static void drawMenuMode() {
    // Определяем цвет фона в зависимости от текущего режима (для справки)
    uint16_t bgColor = (sysData.mode == 0) ? COLOR_BLUE : COLOR_GREEN;
    
    clearScreen(bgColor);
    
    float currentTemp = sensors[3].found ? sensors[3].temp : 0.0f;
    float baseTemp = guildBaseTemp;
    
    tft.setTextFont(FONT_DELTA);
    
    // Пункт 0: "--- MODE ---" (возврат)
    uint16_t bgColor0 = (selectedItem == 0) ? MENU_SELECT_BG : bgColor;
    uint16_t textColor0 = (selectedItem == 0) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor0);
    tft.setTextColor(textColor0, bgColor0);
    tft.setCursor(40, MENU_START_Y + 10);
    tft.print("--- MODE ---");
    
    // Пункт 1: MODE1 (подсвечивается синим, если выбран для подтверждения)
    uint16_t mode1Color = (selectedMode == 0) ? COLOR_BLUE : bgColor;
    uint16_t bgColor1 = (selectedItem == 1) ? mode1Color : bgColor;
    uint16_t textColor1 = (selectedItem == 1 && selectedMode == 0) ? COLOR_WHITE : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + MENU_ITEM_HEIGHT + 10, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor1);
    tft.setTextColor(textColor1, bgColor1);
    tft.setCursor(20, MENU_START_Y + MENU_ITEM_HEIGHT + 20);
    tft.print("MODE1");
    tft.setCursor(150, MENU_START_Y + MENU_ITEM_HEIGHT + 20);
    tft.printf("%05.2f", currentTemp);
    
    // Пункт 2: MODE2 (подсвечивается зелёным, если выбран для подтверждения)
    uint16_t mode2Color = (selectedMode == 1) ? COLOR_GREEN : bgColor;
    uint16_t bgColor2 = (selectedItem == 2) ? mode2Color : bgColor;
    uint16_t textColor2 = (selectedItem == 2 && selectedMode == 1) ? COLOR_WHITE : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor2);
    tft.setTextColor(textColor2, bgColor2);
    tft.setCursor(20, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.print("MODE2");
    tft.setCursor(150, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.printf("%05.2f", baseTemp);
    
    // Пункт 3: OK (подтверждение)
    uint16_t bgColor3 = (selectedItem == 3) ? MENU_SELECT_BG : bgColor;
    uint16_t textColor3 = (selectedItem == 3) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + 3*(MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor3);
    tft.setTextColor(textColor3, bgColor3);
    tft.setCursor(80, MENU_START_Y + 3*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.print("OK");
    
    tft.setTextColor(MENU_TEXT_COLOR, bgColor);
    tft.setCursor(10, 280);
    tft.print("Select: turn  Choose: click  Back: --- MODE ---");
}

/**
 * Отрисовка меню регулировки громкости MP3
 */
static void drawMenuVolume() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("VOLUME");
    
    // Полоска громкости
    uint8_t vol = 15; // TODO: получить реальную громкость
    int barWidth = map(vol, 0, 30, 0, 200);
    
    tft.fillRect(20, 120, 200, 20, TFT_DARKGREY);
    tft.fillRect(20, 120, barWidth, 20, COLOR_CYAN);
    
    tft.setCursor(20, 150);
    tft.printf("%d/30", vol);
    
    tft.setCursor(10, 280);
    tft.print("Turn: change  Click: confirm");
}

/**
 * Отрисовка меню калибровки
 */
static void drawMenuCalib() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("CALIBRATION");
    
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
    tft.print("Turn: select  Hold+turn: adjust  Click: confirm");
}

/**
 * Отрисовка меню настроек
 */
static void drawMenuSettings() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    
    tft.setCursor(20, 80);
    tft.print("SETTINGS");
    
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
    tft.print("Turn: select  Hold+turn: change  Click: confirm");
}

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void menu_init() {
    currentState = MENU_STATE_MAIN;
    selectedItem = 0;
    selectedMode = (sysData.mode == 0) ? 0 : 1;
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
    // ОБРАБОТКА СОБЫТИЙ В ЗАВИСИМОСТИ ОТ СОСТОЯНИЯ
    // ========================================================================
    switch (currentState) {
        
        // ========================= ГЛАВНЫЙ ЭКРАН =========================
        case MENU_STATE_MAIN:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_TOP;
                selectedItem = 0;
                drawMenuTop();
                Serial.println("[MENU] Переход в TOP меню");
            }
            break;
        
        // ========================= ВЕРХНЕЕ МЕНЮ =========================
        case MENU_STATE_TOP:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                drawMenuTop();
            }
            else if (event == EVENT_BUTTON_CLICK) {
                switch (selectedItem) {
                    case 0: // MODE
                        currentState = MENU_STATE_MODE_SELECT;
                        selectedItem = 1;  // По умолчанию выбираем MODE1
                        selectedMode = (sysData.mode == 0) ? 0 : 1;
                        drawMenuMode();
                        Serial.println("[MENU] Переход в MODE");
                        break;
                    case 1: // VOLUME
                        currentState = MENU_STATE_MP3_VOL;
                        selectedItem = 0;
                        drawMenuVolume();
                        Serial.println("[MENU] Переход в VOLUME");
                        break;
                    case 2: // CALIB
                        currentState = MENU_STATE_CALIB;
                        selectedItem = 0;
                        drawMenuCalib();
                        Serial.println("[MENU] Переход в CALIB");
                        break;
                    case 3: // SETTINGS
                        currentState = MENU_STATE_SETTINGS;
                        selectedItem = 0;
                        drawMenuSettings();
                        Serial.println("[MENU] Переход в SETTINGS");
                        break;
                }
            }
            break;
        
        // ========================= ВЫБОР РЕЖИМА (MODE) =========================
        case MENU_STATE_MODE_SELECT:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                // Поворот - переключение выбранного пункта (0,1,2,3)
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                
                // Если перешли на MODE1 или MODE2, автоматически делаем их выбранными для подтверждения
                if (selectedItem == 1) {
                    selectedMode = 0;  // Выбрали MODE1
                } else if (selectedItem == 2) {
                    selectedMode = 1;  // Выбрали MODE2
                }
                
                drawMenuMode();
                Serial.printf("[MENU] MODE: выбран пункт %d, выбранный режим: %d\n", selectedItem, selectedMode);
            }
            else if (event == EVENT_BUTTON_CLICK) {
                if (selectedItem == 0) {
                    // Нажатие на "--- MODE ---" - возврат в TOP
                    currentState = MENU_STATE_TOP;
                    selectedItem = 0;
                    drawMenuTop();
                    Serial.println("[MENU] Возврат в TOP меню");
                }
                else if (selectedItem == 1) {
                    // Нажатие на MODE1 - запоминаем выбор
                    selectedMode = 0;
                    drawMenuMode();  // Перерисовываем с подсветкой
                    Serial.println("[MENU] Выбран MODE1 (ожидает подтверждения)");
                }
                else if (selectedItem == 2) {
                    // Нажатие на MODE2 - запоминаем выбор
                    selectedMode = 1;
                    drawMenuMode();  // Перерисовываем с подсветкой
                    Serial.println("[MENU] Выбран MODE2 (ожидает подтверждения)");
                }
                else if (selectedItem == 3) {
                    // Нажатие на OK - подтверждение выбора
                    uint8_t newMode = selectedMode;
                    Serial.printf("[MENU] Подтверждение: переключение в режим %d\n", newMode);
                    resetDisplayState(newMode);
                    currentState = MENU_STATE_MAIN;
                    forceDisplayRedraw = true;
                }
            }
            break;
        
        // ========================= РЕГУЛИРОВКА ГРОМКОСТИ =========================
        case MENU_STATE_MP3_VOL:
            if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
                // Изменение громкости
                // TODO: реализовать
                Serial.println("[MENU] Изменение громкости (TODO)");
            }
            else if (event == EVENT_BUTTON_CLICK) {
                // Подтверждение, возврат в TOP
                currentState = MENU_STATE_TOP;
                selectedItem = 1;  // Возвращаемся к пункту VOLUME
                drawMenuTop();
                Serial.println("[MENU] Возврат в TOP из VOLUME");
            }
            break;
        
        // ========================= КАЛИБРОВКА =========================
        case MENU_STATE_CALIB:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                drawMenuCalib();
            }
            else if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
                int direction = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
                if (selectedItem >= 0 && selectedItem < 4) {
                    calibrationOffsets[selectedItem] += 0.05f * direction;
                    drawMenuCalib();
                }
            }
            else if (event == EVENT_BUTTON_CLICK) {
                // Подтверждение, возврат в TOP
                currentState = MENU_STATE_TOP;
                selectedItem = 2;  // Возвращаемся к пункту CALIB
                drawMenuTop();
                Serial.println("[MENU] Возврат в TOP из CALIB");
            }
            break;
        
        // ========================= НАСТРОЙКИ =========================
        case MENU_STATE_SETTINGS:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 3;
                } else {
                    selectedItem = (selectedItem == 0) ? 2 : selectedItem - 1;
                }
                drawMenuSettings();
            }
            else if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
                int direction = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
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
            }
            else if (event == EVENT_BUTTON_CLICK) {
                // Подтверждение, возврат в TOP
                currentState = MENU_STATE_TOP;
                selectedItem = 3;  // Возвращаемся к пункту SETTINGS
                drawMenuTop();
                Serial.println("[MENU] Возврат в TOP из SETTINGS");
            }
            break;
        
        default:
            break;
    }
}