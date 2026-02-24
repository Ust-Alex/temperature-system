/** * @file menu_drawing.cpp
 * @brief РЕАЛИЗАЦИЯ ГРАФИКИ МЕНЮ (ФИНАЛЬНАЯ ВЕРСИЯ)
 * 
 * @version 2.0
 * @details Все функции получают параметры и не хранят состояние.
 *          Внутренний кэш используется ТОЛЬКО для оптимизации,
 *          логика в него не вмешивается.
 */

#include "menu_drawing.h"
#include "globals.h"
#include "sensors.h"
#include "mp3_player.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"

extern TFT_eSPI tft;
extern float guildBaseTemp;

// ============================================================================
// ВНУТРЕННИЙ КЭШ (ТОЛЬКО ДЛЯ ОПТИМИЗАЦИИ, НЕ ДЛЯ ХРАНЕНИЯ СОСТОЯНИЯ)
// ============================================================================
static uint8_t lastSelectedItem = 255;
static uint8_t lastVolume = 255;
static uint8_t lastSelectedMode = 255;
static uint16_t lastBgColor = 0;

// ============================================================================
// СБРОС КЭША
// ============================================================================
void drawing_reset_cache() {
    lastSelectedItem = 255;
    lastVolume = 255;
    lastSelectedMode = 255;
    lastBgColor = 0;
}

void drawing_reset_volume_cache() {
    lastVolume = 255;
    lastSelectedItem = 255;
}

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ
// ============================================================================
static void clearScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    vTaskDelay(pdMS_TO_TICKS(10));
    drawing_reset_cache();
    lastBgColor = bgColor;
}

// ============================================================================
// ПОЛНАЯ ОТРИСОВКА: ВЕРХНЕЕ МЕНЮ
// ============================================================================
void drawMenuTop(uint8_t selectedItem) {
    clearScreen(MENU_BG_COLOR);
    tft.setTextFont(FONT_DELTA);

    const char* items[] = { "MODE", "VOLUME", "CALIB", "SETTINGS" };

    for (int i = 0; i < 4; i++) {
        uint16_t bgColor = (i == selectedItem) ? MENU_SELECT_BG : MENU_BG_COLOR;
        uint16_t textColor = (i == selectedItem) ? MENU_SELECT_TEXT : MENU_TEXT_COLOR;

        int yPos = MENU_START_Y + i * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yPos, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, bgColor);
        tft.setTextColor(textColor, bgColor);
        tft.setCursor(30, yPos + 10);
        tft.print(items[i]);
    }
    
    lastSelectedItem = selectedItem;
}

// ============================================================================
// ПОЛНАЯ ОТРИСОВКА: ЭКРАН ВЫБОРА РЕЖИМА
// ============================================================================
void drawMenuMode(uint8_t selectedItem, uint8_t selectedMode, bool modeConfirmed) {
    uint16_t bgColor = (sysData.mode == 0) ? COLOR_BLUE : COLOR_GREEN;
    clearScreen(bgColor);

    tft.setTextFont(FONT_DELTA);

    const char* items[] = { "--- MODE ---", "MODE1", "MODE2", "OK" };
    int y = MENU_START_Y;

    for (int i = 0; i < 4; i++) {
        uint16_t itemColor = bgColor;
        uint16_t textColor = MENU_TEXT_COLOR;

        if (i == selectedItem) {
            itemColor = MENU_SELECT_BG;
            textColor = MENU_SELECT_TEXT;
        }

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
    
    lastSelectedItem = selectedItem;
    lastSelectedMode = modeConfirmed ? selectedMode : 255;
    lastBgColor = bgColor;
}

// ============================================================================
// ПОЛНАЯ ОТРИСОВКА: ЭКРАН ГРОМКОСТИ
// ============================================================================
void drawMenuVolume(uint8_t selectedItem, uint8_t volume) {
    clearScreen(MENU_BG_COLOR);
    
    // Заголовок (всегда одинаковый, выделяется только курсором)
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    tft.setCursor(30, 30);
    tft.print("---VOLUME---");
    
    // Цифра громкости (крупно по центру)
    tft.setTextFont(FONT_BIG);
    
    // Если курсор на цифре (индекс 1) - подсвечиваем
    if (selectedItem == 1) {
        // Рисуем фон под цифрой
        tft.fillRoundRect(70, 80, 100, 60, 8, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    }
    
    // Центрируем цифру в зависимости от количества знаков
    if (volume < 10) {
        tft.setCursor(100, 95);
    } else {
        tft.setCursor(85, 95);
    }
    tft.printf("%d", volume);
    
    // Кнопка >>>>> 
    tft.setTextFont(FONT_DELTA);
    int yPos = 160;  // Базовая позиция для >>>>>
    
    if (selectedItem == 2) {
        tft.fillRoundRect(40, yPos-5, 160, 30, 6, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    }
    tft.setCursor(80, yPos);
    tft.print(">>>>>");
    
    // Кнопка OK
    yPos = 200;  // Позиция для OK
    
    if (selectedItem == 3) {
        tft.fillRoundRect(40, yPos-5, 160, 30, 6, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    }
    tft.setCursor(100, yPos);
    tft.print("ok");
    
    // Обновляем кэш
    lastSelectedItem = selectedItem;
    lastVolume = volume;
}

// ============================================================================
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ: ЭКРАН ГРОМКОСТИ
// ============================================================================
void updateMenuVolume(uint8_t volume, uint8_t currentItem) {
    uint8_t oldItem = lastSelectedItem;
    
    // 1. Если изменилась громкость - обновляем только цифру
    if (volume != lastVolume) {
        // Стираем старую цифру
        tft.fillRect(70, 80, 100, 60, MENU_BG_COLOR);
        
        // Рисуем новую с правильной подсветкой
        tft.setTextFont(FONT_BIG);
        if (currentItem == 1) {
            tft.fillRoundRect(70, 80, 100, 60, 8, MENU_SELECT_BG);
            tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
        } else {
            tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
        }
        
        if (volume < 10) {
            tft.setCursor(100, 95);
        } else {
            tft.setCursor(85, 95);
        }
        tft.printf("%d", volume);
        tft.setTextFont(FONT_DELTA);
        
        lastVolume = volume;
    }
    
    // 2. Если изменился выбранный пункт - обновляем подсветку элементов
    if (currentItem != oldItem) {
        // Сбрасываем старый элемент
        switch (oldItem) {
            case 1: // Была подсвечена цифра
                tft.fillRect(70, 80, 100, 60, MENU_BG_COLOR);
                tft.setTextFont(FONT_BIG);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                if (lastVolume < 10) {
                    tft.setCursor(100, 95);
                } else {
                    tft.setCursor(85, 95);
                }
                tft.printf("%d", lastVolume);
                tft.setTextFont(FONT_DELTA);
                break;
                
            case 2: // Была подсвечена кнопка >>>>>
                tft.fillRect(40, 155, 160, 30, MENU_BG_COLOR);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                tft.setCursor(80, 160);
                tft.print(">>>>>");
                break;
                
            case 3: // Была подсвечена кнопка ok
                tft.fillRect(40, 195, 160, 30, MENU_BG_COLOR);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                tft.setCursor(100, 200);
                tft.print("ok");
                break;
        }
        
        // Подсвечиваем новый элемент
        switch (currentItem) {
            case 1: // Цифра
                tft.fillRoundRect(70, 80, 100, 60, 8, MENU_SELECT_BG);
                tft.setTextFont(FONT_BIG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                if (lastVolume < 10) {
                    tft.setCursor(100, 95);
                } else {
                    tft.setCursor(85, 95);
                }
                tft.printf("%d", lastVolume);
                tft.setTextFont(FONT_DELTA);
                break;
                
            case 2: // >>>>> 
                tft.fillRoundRect(40, 155, 160, 30, 6, MENU_SELECT_BG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                tft.setCursor(80, 160);
                tft.print(">>>>>");
                break;
                
            case 3: // ok
                tft.fillRoundRect(40, 195, 160, 30, 6, MENU_SELECT_BG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                tft.setCursor(100, 200);
                tft.print("ok");
                break;
        }
        
        lastSelectedItem = currentItem;
    }
}

// ============================================================================
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ: ВЕРХНЕЕ МЕНЮ
// ============================================================================
void updateMenuTopSelection(uint8_t oldItem, uint8_t newItem) {
    const char* items[] = { "MODE", "VOLUME", "CALIB", "SETTINGS" };

    // Стираем старый
    int yOld = MENU_START_Y + oldItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
    tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_BG_COLOR);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    tft.setCursor(30, yOld + 10);
    tft.print(items[oldItem]);

    // Рисуем новый выделенным
    int yNew = MENU_START_Y + newItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
    tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
    tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    tft.setCursor(30, yNew + 10);
    tft.print(items[newItem]);

    lastSelectedItem = newItem;
}

// ============================================================================
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ: ЭКРАН ВЫБОРА РЕЖИМА
// ============================================================================
void updateMenuModeSelection(uint8_t currentItem, uint8_t currentSelectedMode, bool isModeConfirmed) {
    // Защита индексов
    uint8_t safeItem = currentItem;
    uint8_t safeLastItem = lastSelectedItem;
    if (safeItem > 3) safeItem = 0;
    if (safeLastItem > 3 && safeLastItem != 255) safeLastItem = 0;
    if (safeLastItem == 255) safeLastItem = 0;

    // 1. Обновление курсора
    if (safeItem != safeLastItem) {
        int yOld = MENU_START_Y + safeLastItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, lastBgColor);
        tft.setTextColor(MENU_TEXT_COLOR, lastBgColor);
        tft.setCursor(40, yOld + 10);

        if (safeLastItem == 0) tft.print("--- MODE ---");
        else if (safeLastItem == 1) tft.print("MODE1");
        else if (safeLastItem == 2) tft.print("MODE2");
        else if (safeLastItem == 3) tft.print("OK");

        if (safeLastItem == 1) {
            tft.setCursor(150, yOld + 10);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (safeLastItem == 2) {
            tft.setCursor(150, yOld + 10);
            tft.printf("%05.2f", guildBaseTemp);
        }

        int yNew = MENU_START_Y + safeItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
        tft.setCursor(40, yNew + 10);

        if (safeItem == 0) tft.print("--- MODE ---");
        else if (safeItem == 1) tft.print("MODE1");
        else if (safeItem == 2) tft.print("MODE2");
        else if (safeItem == 3) tft.print("OK");

        if (safeItem == 1) {
            tft.setCursor(150, yNew + 10);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (safeItem == 2) {
            tft.setCursor(150, yNew + 10);
            tft.printf("%05.2f", guildBaseTemp);
        }

        lastSelectedItem = safeItem;
    }

    // 2. Обновление цветной подсветки
    if (isModeConfirmed) {
        uint16_t color = (currentSelectedMode == 0) ? COLOR_BLUE : COLOR_GREEN;
        int y = MENU_START_Y + (currentSelectedMode + 1) * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);

        tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, color);
        tft.setTextColor(COLOR_WHITE, color);
        tft.setCursor(40, y + 10);
        tft.print(currentSelectedMode == 0 ? "MODE1" : "MODE2");

        tft.setCursor(150, y + 10);
        if (currentSelectedMode == 0) {
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        } else {
            tft.printf("%05.2f", guildBaseTemp);
        }

        if (safeItem == currentSelectedMode + 1) {
            tft.drawRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        }

        lastSelectedMode = currentSelectedMode;
    } else {
        if (lastSelectedMode != 255) {
            int y = MENU_START_Y + (lastSelectedMode + 1) * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
            tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, lastBgColor);
            tft.setTextColor(MENU_TEXT_COLOR, lastBgColor);
            tft.setCursor(40, y + 10);
            tft.print(lastSelectedMode == 0 ? "MODE1" : "MODE2");

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

