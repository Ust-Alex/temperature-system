/**
 * @file menu_drawing.cpp
 * @brief ГРАФИКА МЕНЮ (С ДОБАВЛЕННЫМ BACK)
 * @version 2.4
 */

#include "menu_drawing.h"
#include "globals.h"
#include "sensors.h"
#include "mp3_player.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"
#include "calibration_simple.h"

extern TFT_eSPI tft;
extern float guildBaseTemp;

static uint8_t lastSelectedItem = 255;
static uint8_t lastVolume = 255;
static uint8_t lastSelectedMode = 255;
static uint16_t lastBgColor = 0;

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

static void clearScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    drawing_reset_cache();
    lastBgColor = bgColor;
}

// ============================================================================
void showMessage(const char* msg, uint16_t delayMs) {
    clearScreen(COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setCursor(30, 110);
    tft.print(msg);
    delay(delayMs);
}

// ============================================================================
// ВЕРХНЕЕ МЕНЮ (5 ПУНКТОВ)
// ============================================================================
void drawMenuTop(uint8_t selectedItem) {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);

    const char* items[] = { "MODE", "VOLUME", "CALIB", "SETTINGS", "BACK" };
    int y = 30;

    for (int i = 0; i < 5; i++) {
        uint16_t bg = (i == selectedItem) ? COLOR_WHITE : COLOR_BLACK;
        uint16_t fg = (i == selectedItem) ? COLOR_BLACK : COLOR_WHITE;

        tft.fillRect(10, y, 220, 28, bg);
        tft.setTextColor(fg, bg);
        tft.setCursor(30, y + 6);
        tft.print(items[i]);
        y += 30;
    }
    lastSelectedItem = selectedItem;
}

void updateMenuTopSelection(uint8_t oldItem, uint8_t newItem) {
    const char* items[] = { "MODE", "VOLUME", "CALIB", "SETTINGS", "BACK" };
    int yBase = 30;
    int step = 30;

    int yOld = yBase + oldItem * step;
    tft.fillRect(10, yOld, 220, 28, COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft.setCursor(30, yOld + 6);
    tft.print(items[oldItem]);

    int yNew = yBase + newItem * step;
    tft.fillRect(10, yNew, 220, 28, COLOR_WHITE);
    tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    tft.setCursor(30, yNew + 6);
    tft.print(items[newItem]);

    lastSelectedItem = newItem;
}

// ============================================================================
// ЭКРАН ВЫБОРА РЕЖИМА (MODE)
// ============================================================================
void drawMenuMode(uint8_t selectedItem, uint8_t selectedMode, bool modeConfirmed) {
    uint16_t bgColor = (sysData.mode == 0) ? COLOR_BLUE : COLOR_GREEN;
    clearScreen(bgColor);
    tft.setTextFont(FONT_DELTA);

    const char* items[] = { "--- MODE ---", "MODE1", "MODE2", "OK" };
    int y = 20;

    for (int i = 0; i < 4; i++) {
        uint16_t itemColor = bgColor;
        uint16_t textColor = COLOR_WHITE;

        if (i == selectedItem) {
            itemColor = COLOR_WHITE;
            textColor = COLOR_BLACK;
        }
        if (i == 1 && selectedMode == 0 && modeConfirmed) {
            itemColor = COLOR_BLUE;
            textColor = COLOR_WHITE;
        }
        if (i == 2 && selectedMode == 1 && modeConfirmed) {
            itemColor = COLOR_GREEN;
            textColor = COLOR_WHITE;
        }

        tft.fillRect(10, y, 220, 30, itemColor);
        tft.setTextColor(textColor, itemColor);
        tft.setCursor(30, y + 8);
        tft.print(items[i]);

        if (i == 1) {
            tft.setCursor(150, y + 8);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (i == 2) {
            tft.setCursor(150, y + 8);
            tft.printf("%05.2f", guildBaseTemp);
        }
        y += 35;
    }

    lastSelectedItem = selectedItem;
    lastSelectedMode = modeConfirmed ? selectedMode : 255;
    lastBgColor = bgColor;
}

void updateMenuModeSelection(uint8_t currentItem, uint8_t currentSelectedMode, bool isModeConfirmed) {
    uint8_t safeItem = currentItem;
    uint8_t safeLastItem = lastSelectedItem;
    if (safeItem > 3) safeItem = 0;
    if (safeLastItem > 3 && safeLastItem != 255) safeLastItem = 0;
    if (safeLastItem == 255) safeLastItem = 0;

    if (safeItem != safeLastItem) {
        int yOld = 20 + safeLastItem * 35;
        tft.fillRect(10, yOld, 220, 30, lastBgColor);
        tft.setTextColor(COLOR_WHITE, lastBgColor);
        tft.setCursor(30, yOld + 8);
        if (safeLastItem == 0) tft.print("--- MODE ---");
        else if (safeLastItem == 1) tft.print("MODE1");
        else if (safeLastItem == 2) tft.print("MODE2");
        else if (safeLastItem == 3) tft.print("OK");

        if (safeLastItem == 1) {
            tft.setCursor(150, yOld + 8);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (safeLastItem == 2) {
            tft.setCursor(150, yOld + 8);
            tft.printf("%05.2f", guildBaseTemp);
        }

        int yNew = 20 + safeItem * 35;
        tft.fillRect(10, yNew, 220, 30, COLOR_WHITE);
        tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
        tft.setCursor(30, yNew + 8);
        if (safeItem == 0) tft.print("--- MODE ---");
        else if (safeItem == 1) tft.print("MODE1");
        else if (safeItem == 2) tft.print("MODE2");
        else if (safeItem == 3) tft.print("OK");

        if (safeItem == 1) {
            tft.setCursor(150, yNew + 8);
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        }
        if (safeItem == 2) {
            tft.setCursor(150, yNew + 8);
            tft.printf("%05.2f", guildBaseTemp);
        }

        lastSelectedItem = safeItem;
    }

    if (isModeConfirmed) {
        uint16_t color = (currentSelectedMode == 0) ? COLOR_BLUE : COLOR_GREEN;
        int y = 20 + (currentSelectedMode + 1) * 35;
        tft.fillRect(10, y, 220, 30, color);
        tft.setTextColor(COLOR_WHITE, color);
        tft.setCursor(30, y + 8);
        tft.print(currentSelectedMode == 0 ? "MODE1" : "MODE2");

        tft.setCursor(150, y + 8);
        if (currentSelectedMode == 0) {
            tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
        } else {
            tft.printf("%05.2f", guildBaseTemp);
        }

        if (safeItem == currentSelectedMode + 1) {
            tft.drawRect(10, y, 220, 30, COLOR_WHITE);
        }
        lastSelectedMode = currentSelectedMode;
    } else {
        if (lastSelectedMode != 255) {
            int y = 20 + (lastSelectedMode + 1) * 35;
            tft.fillRect(10, y, 220, 30, lastBgColor);
            tft.setTextColor(COLOR_WHITE, lastBgColor);
            tft.setCursor(30, y + 8);
            tft.print(lastSelectedMode == 0 ? "MODE1" : "MODE2");

            tft.setCursor(150, y + 8);
            if (lastSelectedMode == 0) {
                tft.printf("%05.2f", sensors[3].found ? sensors[3].temp : 0);
            } else {
                tft.printf("%05.2f", guildBaseTemp);
            }
            lastSelectedMode = 255;
        }
    }
}

// ============================================================================
// ЭКРАН ГРОМКОСТИ (VOLUME)
// ============================================================================
void drawMenuVolume(uint8_t selectedItem, uint8_t volume) {
    clearScreen(COLOR_BLACK);

    tft.setTextFont(FONT_DELTA);
    if (selectedItem == 0) {
        tft.fillRoundRect(20, 20, 200, 30, 4, COLOR_WHITE);
        tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else {
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    tft.setCursor(50, 27);
    tft.print("---VOLUME---");

    tft.setTextFont(FONT_BIG);
    if (selectedItem == 1) {
        tft.fillRoundRect(70, 80, 100, 60, 8, COLOR_WHITE);
        tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else {
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    if (volume < 10) {
        tft.setCursor(100, 95);
    } else {
        tft.setCursor(85, 95);
    }
    tft.printf("%d", volume);

    tft.setTextFont(FONT_DELTA);
    int y = 160;
    if (selectedItem == 2) {
        tft.fillRoundRect(40, y - 5, 160, 30, 6, COLOR_WHITE);
        tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else {
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    tft.setCursor(80, y);
    tft.print(">>>>>");

    y = 200;
    if (selectedItem == 3) {
        tft.fillRoundRect(40, y - 5, 160, 30, 6, COLOR_WHITE);
        tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else {
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    tft.setCursor(100, y);
    tft.print("ok");

    lastSelectedItem = selectedItem;
    lastVolume = volume;
}

void updateMenuVolume(uint8_t volume, uint8_t currentItem) {
    uint8_t oldItem = lastSelectedItem;

    if (volume != lastVolume) {
        tft.fillRect(70, 80, 100, 60, COLOR_BLACK);
        tft.setTextFont(FONT_BIG);
        if (currentItem == 1) {
            tft.fillRoundRect(70, 80, 100, 60, 8, COLOR_WHITE);
            tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
        } else {
            tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
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

    if (currentItem != oldItem) {
        switch (oldItem) {
            case 0:
                tft.fillRect(20, 20, 200, 30, COLOR_BLACK);
                tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
                tft.setCursor(50, 27);
                tft.print("---VOLUME---");
                break;
            case 1:
                tft.fillRect(70, 80, 100, 60, COLOR_BLACK);
                tft.setTextFont(FONT_BIG);
                tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
                if (lastVolume < 10) tft.setCursor(100, 95);
                else tft.setCursor(85, 95);
                tft.printf("%d", lastVolume);
                tft.setTextFont(FONT_DELTA);
                break;
            case 2:
                tft.fillRect(40, 155, 160, 30, COLOR_BLACK);
                tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
                tft.setCursor(80, 160);
                tft.print(">>>>>");
                break;
            case 3:
                tft.fillRect(40, 195, 160, 30, COLOR_BLACK);
                tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
                tft.setCursor(100, 200);
                tft.print("ok");
                break;
        }

        switch (currentItem) {
            case 0:
                tft.fillRoundRect(20, 20, 200, 30, 4, COLOR_WHITE);
                tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
                tft.setCursor(50, 27);
                tft.print("---VOLUME---");
                break;
            case 1:
                tft.fillRoundRect(70, 80, 100, 60, 8, COLOR_WHITE);
                tft.setTextFont(FONT_BIG);
                tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
                if (lastVolume < 10) tft.setCursor(100, 95);
                else tft.setCursor(85, 95);
                tft.printf("%d", lastVolume);
                tft.setTextFont(FONT_DELTA);
                break;
            case 2:
                tft.fillRoundRect(40, 155, 160, 30, 6, COLOR_WHITE);
                tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
                tft.setCursor(80, 160);
                tft.print(">>>>>");
                break;
            case 3:
                tft.fillRoundRect(40, 195, 160, 30, 6, COLOR_WHITE);
                tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
                tft.setCursor(100, 200);
                tft.print("ok");
                break;
        }
        lastSelectedItem = currentItem;
    }
}

// ============================================================================
// МЕНЮ КАЛИБРОВКИ
// ============================================================================
void drawMenuCalib(uint8_t selectedItem) {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);

    const char* items[] = { "STATUS", "RESET", "AUTO", "BACK" };
    int y = 40;

    for (int i = 0; i < 4; i++) {
        uint16_t bg = (i == selectedItem) ? COLOR_WHITE : COLOR_BLACK;
        uint16_t fg = (i == selectedItem) ? COLOR_BLACK : COLOR_WHITE;

        tft.fillRect(10, y, 220, 30, bg);
        tft.setTextColor(fg, bg);
        tft.setCursor(30, y + 8);
        tft.print(items[i]);
        y += 35;
    }
    lastSelectedItem = selectedItem;
}

void updateMenuCalibSelection(uint8_t oldItem, uint8_t newItem) {
    const char* items[] = { "STATUS", "RESET", "AUTO", "BACK" };

    int yOld = 40 + oldItem * 35;
    tft.fillRect(10, yOld, 220, 30, COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft.setCursor(30, yOld + 8);
    tft.print(items[oldItem]);

    int yNew = 40 + newItem * 35;
    tft.fillRect(10, yNew, 220, 30, COLOR_WHITE);
    tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    tft.setCursor(30, yNew + 8);
    tft.print(items[newItem]);

    lastSelectedItem = newItem;
}

void drawCalibStatus() {
    clearScreen(COLOR_BLACK);
    tft.setTextFont(FONT_DELTA);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);

    int y = 20;
    for (int i = 0; i < 4; i++) {
        if (!sensors[i].found) continue;

        float raw = sensors[i].temp;
        float offset = settings_get_offset(i);
        float cal = raw + offset;

        char line[30];
        if (offset > 0) {
            snprintf(line, sizeof(line), "%d %05.2f->%05.2f +%.2f",
                     i, raw, cal, offset);
        } else {
            snprintf(line, sizeof(line), "%d %05.2f->%05.2f %.2f",
                     i, raw, cal, offset);
        }

        tft.setCursor(3, y);
        tft.print(line);
        y += 25;
    }

    tft.fillRect(70, 200, 100, 30, COLOR_WHITE);
    tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    tft.setCursor(100, 208);
    tft.print("BACK");
}