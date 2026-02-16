/**
 * ============================================================================
 * ФАЙЛ: menu_engine.cpp
 * РЕАЛИЗАЦИЯ УПРАВЛЕНИЯ МЕНЮ ЭНКОДЕРА
 * 
 * ВЕРСИЯ: 3.1 (ДОБАВЛЕНО МЕНЮ VOLUME)
 * 
 * ОСОБЕННОСТИ:
 * - MODE: выбор режима с визуальным подтверждением
 * - VOLUME: регулировка громкости MP3 с тестовым воспроизведением
 * - TOP: верхнее меню для навигации
 * - Таймаут 30 сек возврат в MAIN
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
#define MENU_START_Y     60             // Начальная Y-координата

// Цвета текста в меню
#define MENU_TEXT_COLOR   COLOR_WHITE
#define MENU_SELECT_BG    COLOR_WHITE      // Для выделения пунктов белым
#define MENU_SELECT_TEXT  COLOR_BLACK

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ (STATIC)
// ============================================================================
static MenuState_t currentState = MENU_STATE_MAIN;
static uint32_t lastActivityTime = 0;
static uint8_t selectedItem = 0;         // Выбранный пункт в текущем подменю
static uint8_t selectedMode = 0;          // 0 = MODE1, 1 = MODE2 (для подтверждения)
static bool modeConfirmed = false;        // Флаг: был ли выбран режим (фон изменён)

// Внешние объекты
extern TFT_eSPI tft;
extern QueueHandle_t eventQueue;
extern uint8_t guildColorState;
extern float guildBaseTemp;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

/**
 * Полная очистка экрана
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

// ============================================================================
// ФУНКЦИИ ОТРИСОВКИ МЕНЮ
// ============================================================================

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
    // Определяем цвет фона:
    // - Если режим подтверждён (modeConfirmed = true) → цвет выбранного режима
    // - Иначе → цвет текущего режима системы
    uint16_t bgColor;
    if (modeConfirmed) {
        bgColor = (selectedMode == 0) ? COLOR_BLUE : COLOR_GREEN;
    } else {
        bgColor = (sysData.mode == 0) ? COLOR_BLUE : COLOR_GREEN;
    }
    
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
    
    // Пункт 1: MODE1
    // Если курсор на MODE1, подсвечиваем его цветом режима (синим)
    uint16_t mode1Color = (selectedItem == 1) ? COLOR_BLUE : bgColor;
    uint16_t textColor1 = (selectedItem == 1) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    // Но если фон уже синий и курсор не на MODE1, делаем текст чёрным для контраста
    if (bgColor == COLOR_BLUE && selectedItem != 1) {
        textColor1 = COLOR_BLACK;
    }
    
    tft.fillRect(10, MENU_START_Y + MENU_ITEM_HEIGHT + 10, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, mode1Color);
    tft.setTextColor(textColor1, mode1Color);
    tft.setCursor(20, MENU_START_Y + MENU_ITEM_HEIGHT + 20);
    tft.print("MODE1");
    tft.setCursor(150, MENU_START_Y + MENU_ITEM_HEIGHT + 20);
    tft.printf("%05.2f", currentTemp);
    
    // Пункт 2: MODE2
    // Если курсор на MODE2, подсвечиваем его цветом режима (зелёным)
    uint16_t mode2Color = (selectedItem == 2) ? COLOR_GREEN : bgColor;
    uint16_t textColor2 = (selectedItem == 2) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    // Если фон уже зелёный и курсор не на MODE2, делаем текст чёрным
    if (bgColor == COLOR_GREEN && selectedItem != 2) {
        textColor2 = COLOR_BLACK;
    }
    
    tft.fillRect(10, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, mode2Color);
    tft.setTextColor(textColor2, mode2Color);
    tft.setCursor(20, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.print("MODE2");
    tft.setCursor(150, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.printf("%05.2f", baseTemp);
    
    // Пункт 3: OK
    uint16_t bgColor3 = (selectedItem == 3) ? MENU_SELECT_BG : bgColor;
    uint16_t textColor3 = (selectedItem == 3) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + 3*(MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor3);
    tft.setTextColor(textColor3, bgColor3);
    tft.setCursor(80, MENU_START_Y + 3*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.print("OK");
    
    // Подсказка внизу
    tft.setTextColor(MENU_TEXT_COLOR, bgColor);
    tft.setCursor(10, 280);
    tft.print("Select: turn  Choose: click  Back: --- MODE ---");
}

/**
 * Отрисовка меню регулировки громкости (VOLUME)
 * Пункты: 0 = "--- VOLUME ---", 1 = ползунок, 2 = Play, 3 = OK
 */
static void drawMenuVolume() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    
    uint8_t vol = settings_get_mp3_volume();
    
    // Пункт 0: "--- VOLUME ---" (возврат)
    uint16_t bgColor0 = (selectedItem == 0) ? MENU_SELECT_BG : COLOR_BLACK;
    uint16_t textColor0 = (selectedItem == 0) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor0);
    tft.setTextColor(textColor0, bgColor0);
    tft.setCursor(30, MENU_START_Y + 10);
    tft.print("--- VOLUME ---");
    
    // Пункт 1: Ползунок (интерактивный)
    uint16_t bgColor1 = (selectedItem == 1) ? MENU_SELECT_BG : COLOR_BLACK;
    uint16_t textColor1 = (selectedItem == 1) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + MENU_ITEM_HEIGHT + 10, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor1);
    tft.setTextColor(textColor1, bgColor1);
    
    // Рисуем ползунок
    int barX = 20;
    int barY = MENU_START_Y + MENU_ITEM_HEIGHT + 20;
    int barWidth = 180;
    int barHeight = 15;
    
    tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
    int fillWidth = map(vol, 0, 30, 0, barWidth);
    tft.fillRect(barX, barY, fillWidth, barHeight, COLOR_CYAN);
    
    // Цифры громкости
    tft.setCursor(barX + barWidth + 10, barY - 2);
    tft.printf("%d/30", vol);
    
    // Пункт 2: Кнопка Play (>>>>)
    uint16_t bgColor2 = (selectedItem == 2) ? MENU_SELECT_BG : COLOR_BLACK;
    uint16_t textColor2 = (selectedItem == 2) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor2);
    tft.setTextColor(textColor2, bgColor2);
    tft.setCursor(80, MENU_START_Y + 2*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.print(">>>>  PLAY  >>>>");
    
    // Пункт 3: OK
    uint16_t bgColor3 = (selectedItem == 3) ? MENU_SELECT_BG : COLOR_BLACK;
    uint16_t textColor3 = (selectedItem == 3) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;
    
    tft.fillRect(10, MENU_START_Y + 3*(MENU_ITEM_HEIGHT + 10), MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor3);
    tft.setTextColor(textColor3, bgColor3);
    tft.setCursor(100, MENU_START_Y + 3*(MENU_ITEM_HEIGHT + 10) + 10);
    tft.print("OK");
    
    // Подсказка
    tft.setTextColor(MENU_TEXT_COLOR, COLOR_BLACK);
    tft.setCursor(10, 280);
    tft.print("Hold+turn: volume  Play: test  OK: save");
}

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void menu_init() {
    currentState = MENU_STATE_MAIN;
    selectedItem = 0;
    selectedMode = sysData.mode;  // При входе selectedMode = текущий режим
    modeConfirmed = false;
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
            modeConfirmed = false;
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
                modeConfirmed = false;
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
                        selectedItem = 1;  // Курсор на MODE1
                        selectedMode = sysData.mode;
                        modeConfirmed = false;
                        drawMenuMode();
                        Serial.println("[MENU] Переход в MODE");
                        break;
                    case 1: // VOLUME
                        currentState = MENU_STATE_MP3_VOL;
                        selectedItem = 1;  // Курсор на ползунке
                        drawMenuVolume();
                        Serial.println("[MENU] Переход в VOLUME");
                        break;
                    case 2: // CALIB
                        currentState = MENU_STATE_CALIB;
                        selectedItem = 0;
                        // drawMenuCalib(); // TODO
                        Serial.println("[MENU] Переход в CALIB (в разработке)");
                        break;
                    case 3: // SETTINGS
                        currentState = MENU_STATE_SETTINGS;
                        selectedItem = 0;
                        // drawMenuSettings(); // TODO
                        Serial.println("[MENU] Переход в SETTINGS (в разработке)");
                        break;
                }
            }
            break;
        
        // ========================= ВЫБОР РЕЖИМА (MODE) =========================
        case MENU_STATE_MODE_SELECT:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t oldItem = selectedItem;
                
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                
                drawMenuMode();
                Serial.printf("[MENU] MODE: пункт %d -> %d, selectedMode=%d, modeConfirmed=%d\n", 
                              oldItem, selectedItem, selectedMode, modeConfirmed);
            }
            else if (event == EVENT_BUTTON_CLICK) {
                if (selectedItem == 0) {
                    currentState = MENU_STATE_TOP;
                    selectedItem = 0;
                    modeConfirmed = false;
                    drawMenuTop();
                    Serial.println("[MENU] Возврат в TOP меню");
                }
                else if (selectedItem == 1) {
                    if (sysData.mode != 0) {
                        selectedMode = 0;
                        modeConfirmed = true;
                        drawMenuMode();
                        Serial.println("[MENU] Выбран MODE1 (фон синий)");
                    } else {
                        Serial.println("[MENU] Вы уже в MODE1, выберите MODE2");
                    }
                }
                else if (selectedItem == 2) {
                    if (sysData.mode != 1) {
                        selectedMode = 1;
                        modeConfirmed = true;
                        drawMenuMode();
                        Serial.println("[MENU] Выбран MODE2 (фон зелёный)");
                    } else {
                        Serial.println("[MENU] Вы уже в MODE2, выберите MODE1");
                    }
                }
                else if (selectedItem == 3) {
                    if (modeConfirmed) {
                        Serial.printf("[MENU] Подтверждение: переключение в режим %d\n", selectedMode);
                        resetDisplayState(selectedMode);
                        currentState = MENU_STATE_MAIN;
                        modeConfirmed = false;
                        forceDisplayRedraw = true;
                        Serial.println("[MENU] Выход в MAIN");
                    } else {
                        Serial.println("[MENU] Сначала выберите режим (MODE1/MODE2)");
                    }
                }
            }
            break;
        
        // ========================= РЕГУЛИРОВКА ГРОМКОСТИ (VOLUME) =========================
        case MENU_STATE_MP3_VOL:
            if (event == EVENT_ENCODER_LEFT || event == EVENT_ENCODER_RIGHT) {
                uint8_t oldItem = selectedItem;
                if (event == EVENT_ENCODER_RIGHT) {
                    selectedItem = (selectedItem + 1) % 4;
                } else {
                    selectedItem = (selectedItem == 0) ? 3 : selectedItem - 1;
                }
                drawMenuVolume();
                Serial.printf("[MENU] VOLUME: пункт %d -> %d\n", oldItem, selectedItem);
            }
            else if (event == EVENT_HOLD_LEFT || event == EVENT_HOLD_RIGHT) {
                // Удержание + поворот - изменение громкости (только когда курсор на ползунке)
                if (selectedItem == 1) {
                    int direction = (event == EVENT_HOLD_RIGHT) ? 1 : -1;
                    uint8_t oldVol = settings_get_mp3_volume();
                    uint8_t newVol = oldVol + direction;
                    if (newVol >= 0 && newVol <= 30) {
                        settings_set_mp3_volume(newVol);
                        drawMenuVolume();
                        Serial.printf("[MENU] Громкость: %d -> %d\n", oldVol, newVol);
                    }
                }
            }
            else if (event == EVENT_BUTTON_CLICK) {
                if (selectedItem == 0) {
                    currentState = MENU_STATE_TOP;
                    selectedItem = 1;
                    drawMenuTop();
                    Serial.println("[MENU] Выход из VOLUME без изменений");
                }
                else if (selectedItem == 1) {
                    Serial.println("[MENU] Используйте удержание для изменения громкости");
                }
                else if (selectedItem == 2) {
                    Serial.println("[MENU] Тестовое воспроизведение 0001.mp3");
                    Mp3Command_t playCmd = {MP3_CMD_PLAY_TRACK, 1};
                    sendMP3Command(playCmd);
                }
                else if (selectedItem == 3) {
                    Serial.println("[MENU] Громкость сохранена, выход в TOP");
                    currentState = MENU_STATE_TOP;
                    selectedItem = 1;
                    drawMenuTop();
                }
            }
            break;
        
        // ========================= ОСТАЛЬНЫЕ МЕНЮ (В РАЗРАБОТКЕ) =========================
        case MENU_STATE_CALIB:
        case MENU_STATE_SETTINGS:
        case MENU_STATE_WIFI:
            if (event == EVENT_BUTTON_CLICK) {
                currentState = MENU_STATE_TOP;
                selectedItem = 0;
                drawMenuTop();
                Serial.println("[MENU] Возврат в TOP (заглушка)");
            }
            break;
        
        default:
            break;
    }
}