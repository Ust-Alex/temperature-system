/**
 * @file menu_engine.h
 * @brief ЗАГОЛОВОЧНЫЙ ФАЙЛ МОДУЛЯ УПРАВЛЕНИЯ МЕНЮ
 * @version 1.1
 */

#ifndef MENU_ENGINE_H
#define MENU_ENGINE_H

#include <Arduino.h>
#include "encoder_engine.h"

typedef enum {
    MENU_STATE_MAIN = 0,
    MENU_STATE_TOP,
    MENU_STATE_MODE_SELECT,
    MENU_STATE_CALIB,           // Основное меню калибровки
    MENU_STATE_CALIB_STATUS,    // НОВОЕ: экран с таблицей offset
    MENU_STATE_STATUS,
    MENU_STATE_SETTINGS,
    MENU_STATE_MP3_VOL,
    MENU_STATE_WIFI,
    MENU_STATE_COUNT
} MenuState_t;

void menu_init();
void menu_handle_event(EncoderEvent_t event);
void menu_check_timeout();
MenuState_t menu_get_current_state();
bool menu_is_active();

#endif