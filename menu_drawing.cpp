/**
 * @file menu_drawing.cpp
 * @brief РЕАЛИЗАЦИЯ ГРАФИКИ МЕНЮ (ПОЛНАЯ ОПТИМИЗИРОВАННАЯ ВЕРСИЯ)
 * 
 * @version 2.2
 * @date 2026
 * 
 * @details ОСОБЕННОСТИ:
 *          - Все функции получают параметры и не хранят состояние.
 *          - Внутренний кэш используется ТОЛЬКО для оптимизации частичной перерисовки.
 *          - Убрана задержка vTaskDelay в clearScreen для максимальной скорости отклика.
 *          - Полная реализация updateMenuModeSelection.
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
// ВНУТРЕННИЙ КЭШ (ДЛЯ ОПТИМИЗАЦИИ ЧАСТИЧНОЙ ПЕРЕРИСОВКИ)
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
// ВСПОМОГАТЕЛЬНАЯ (ОПТИМИЗИРОВАНА - ЗАДЕРЖКА УДАЛЕНА)
// ============================================================================
static void clearScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    // vTaskDelay(pdMS_TO_TICKS(10)); - УДАЛЕНО для ускорения
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
    
    // Заголовок (индекс 0)
    tft.setTextFont(FONT_DELTA);
    if (selectedItem == 0) {
        tft.fillRoundRect(20, 20, 200, 30, 4, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    }
    tft.setCursor(50, 27);
    tft.print("---VOLUME---");
    
    // Цифра громкости (индекс 1)
    tft.setTextFont(FONT_BIG);
    if (selectedItem == 1) {
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
    
    // Кнопка >>>>> (индекс 2)
    tft.setTextFont(FONT_DELTA);
    int yPos = 160;
    
    if (selectedItem == 2) {
        tft.fillRoundRect(40, yPos-5, 160, 30, 6, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    }
    tft.setCursor(80, yPos);
    tft.print(">>>>>");
    
    // Кнопка OK (индекс 3)
    yPos = 200;
    
    if (selectedItem == 3) {
        tft.fillRoundRect(40, yPos-5, 160, 30, 6, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    }
    tft.setCursor(100, yPos);
    tft.print("ok");
    
    lastSelectedItem = selectedItem;
    lastVolume = volume;
}

// ============================================================================
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ: ЭКРАН ГРОМКОСТИ
// ============================================================================
void updateMenuVolume(uint8_t volume, uint8_t currentItem) {
    uint8_t oldItem = lastSelectedItem;
    
    // 1. Обновление цифры
    if (volume != lastVolume) {
        tft.fillRect(70, 80, 100, 60, MENU_BG_COLOR);
        tft.setTextFont(FONT_BIG);
        if (currentItem == 1) {
            tft.fillRoundRect(70, 80, 100, 60, 8, MENU_SELECT_BG);
            tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
        } else {
            tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
        }
        if (volume < 10) tft.setCursor(100, 95);
        else tft.setCursor(85, 95);
        tft.printf("%d", volume);
        tft.setTextFont(FONT_DELTA);
        lastVolume = volume;
    }
    
    // 2. Обновление подсветки
    if (currentItem != oldItem) {
        // Сброс старого элемента
        switch (oldItem) {
            case 0:
                tft.fillRect(20, 20, 200, 30, MENU_BG_COLOR);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                tft.setCursor(50, 27);
                tft.print("---VOLUME---");
                break;
            case 1:
                tft.fillRect(70, 80, 100, 60, MENU_BG_COLOR);
                tft.setTextFont(FONT_BIG);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                if (lastVolume < 10) tft.setCursor(100, 95);
                else tft.setCursor(85, 95);
                tft.printf("%d", lastVolume);
                tft.setTextFont(FONT_DELTA);
                break;
            case 2:
                tft.fillRect(40, 155, 160, 30, MENU_BG_COLOR);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                tft.setCursor(80, 160);
                tft.print(">>>>>");
                break;
            case 3:
                tft.fillRect(40, 195, 160, 30, MENU_BG_COLOR);
                tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
                tft.setCursor(100, 200);
                tft.print("ok");
                break;
        }
        
        // Подсветка нового элемента
        switch (currentItem) {
            case 0:
                tft.fillRoundRect(20, 20, 200, 30, 4, MENU_SELECT_BG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                tft.setCursor(50, 27);
                tft.print("---VOLUME---");
                break;
            case 1:
                tft.fillRoundRect(70, 80, 100, 60, 8, MENU_SELECT_BG);
                tft.setTextFont(FONT_BIG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                if (lastVolume < 10) tft.setCursor(100, 95);
                else tft.setCursor(85, 95);
                tft.printf("%d", lastVolume);
                tft.setTextFont(FONT_DELTA);
                break;
            case 2:
                tft.fillRoundRect(40, 155, 160, 30, 6, MENU_SELECT_BG);
                tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
                tft.setCursor(80, 160);
                tft.print(">>>>>");
                break;
            case 3:
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

    int yOld = MENU_START_Y + oldItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
    tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_BG_COLOR);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    tft.setCursor(30, yOld + 10);
    tft.print(items[oldItem]);

    int yNew = MENU_START_Y + newItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
    tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
    tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
    tft.setCursor(30, yNew + 10);
    tft.print(items[newItem]);

    lastSelectedItem = newItem;
}

// ============================================================================
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ: ЭКРАН ВЫБОРА РЕЖИМА (ПОЛНАЯ РЕАЛИЗАЦИЯ)
// ============================================================================
void updateMenuModeSelection(uint8_t currentItem, uint8_t currentSelectedMode, bool isModeConfirmed) {
    // Защита индексов
    uint8_t safeItem = currentItem;
    uint8_t safeLastItem = lastSelectedItem;
    if (safeItem > 3) safeItem = 0;
    if (safeLastItem > 3 && safeLastItem != 255) safeLastItem = 0;
    if (safeLastItem == 255) safeLastItem = 0;

    // 1. ОБНОВЛЕНИЕ КУРСОРА (белая рамка)
    if (safeItem != safeLastItem) {
        // Полностью перерисовываем старый пункт с фоном меню
        int yOld = MENU_START_Y + safeLastItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yOld, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, lastBgColor);
        tft.setTextColor(MENU_TEXT_COLOR, lastBgColor);
        tft.setCursor(40, yOld + 10);

        if (safeLastItem == 0) tft.print("--- MODE ---");
        else if (safeLastItem == 1) tft.print("MODE1");
        else if (safeLastItem == 2) tft.print("MODE2");
        else if (safeLastItem == 3) tft.print("OK");

        // Если старый пункт - MODE1 или MODE2, перерисовываем температуру
        if (safeLastItem == 1) {
            tft.setCursor(150, yOld + 10);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (safeLastItem == 2) {
            tft.setCursor(150, yOld + 10);
            tft.printf("%05.2f", guildBaseTemp);
        }

        // Рисуем новый пункт с белым фоном
        int yNew = MENU_START_Y + safeItem * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);
        tft.fillRect(10, yNew, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        tft.setTextColor(MENU_SELECT_TEXT, MENU_SELECT_BG);
        tft.setCursor(40, yNew + 10);

        if (safeItem == 0) tft.print("--- MODE ---");
        else if (safeItem == 1) tft.print("MODE1");
        else if (safeItem == 2) tft.print("MODE2");
        else if (safeItem == 3) tft.print("OK");

        // Если новый пункт - MODE1 или MODE2, рисуем температуру
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

    // 2. ОБНОВЛЕНИЕ ЦВЕТНОЙ ПОДСВЕТКИ (выбранный режим)
    if (isModeConfirmed) {
        // Рисуем подсветку для выбранного режима
        uint16_t color = (currentSelectedMode == 0) ? COLOR_BLUE : COLOR_GREEN;
        int y = MENU_START_Y + (currentSelectedMode + 1) * (MENU_ITEM_HEIGHT + MENU_ITEM_SPACING);

        tft.fillRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, color);
        tft.setTextColor(COLOR_WHITE, color);
        tft.setCursor(40, y + 10);
        tft.print(currentSelectedMode == 0 ? "MODE1" : "MODE2");

        // Рисуем температуру
        tft.setCursor(150, y + 10);
        if (currentSelectedMode == 0) {
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        } else {
            tft.printf("%05.2f", guildBaseTemp);
        }

        // Если курсор сейчас на этом же пункте, нужно поверх нарисовать белую рамку
        if (safeItem == currentSelectedMode + 1) {
            tft.drawRect(10, y, MENU_ITEM_WIDTH, MENU_ITEM_HEIGHT, MENU_SELECT_BG);
        }

        lastSelectedMode = currentSelectedMode;
    } else {
        // Если режим не выбран, стираем старую подсветку
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