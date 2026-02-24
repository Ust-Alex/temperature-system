/**
 * ============================================================================
 * ФАЙЛ: encoder_engine.cpp
 * РЕАЛИЗАЦИЯ РАБОТЫ С ЭНКОДЕРОМ KY-040 (ПОЛНАЯ ПОДДЕРЖКА GYVER)
 * 
 * ВЕРСИЯ: 4.2 (ИСПРАВЛЕНО: УДЕРЖАНИЕ+ПОВОРОТ ЧЕРЕЗ leftH()/rightH())
 * 
 * ОСОБЕННОСТИ:
 * 1. Использует библиотеку EncButton для надёжной обработки
 * 2. Отправляет ВСЕ события в очередь
 * 3. Поддерживает: поворот (left/right), поворот с удержанием (leftH/rightH),
 *    клик, двойной клик, удержание без поворота
 * 4. Таймер неактивности пользователя
 * 5. Подробная отладка всех событий
 * ============================================================================
 */

#include "encoder_engine.h"
#include "system_config.h"
#include <EncButton.h>

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

// Объект энкодера из библиотеки EncButton
// Параметры: CLK, DT, SW, режим пинов энкодера (INPUT), режим пина кнопки (INPUT_PULLUP)
EncButton enc(ENCODER_CLK, ENCODER_DT, ENCODER_SW, INPUT, INPUT_PULLUP);

// Время последней активности пользователя (для таймаута)
static uint32_t lastActivityTime = 0;

// Очередь событий (внешняя, объявлена в globals.h)
extern QueueHandle_t eventQueue;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void encoder_init() {
    Serial.println("[ENCODER] Инициализация энкодера...");
    
    // НАСТРОЙКА УРОВНЯ СИГНАЛА КНОПКИ
    enc.setBtnLevel(LOW);           // Кнопка замыкает на GND
    
    // НАСТРОЙКА ТАЙМАУТОВ
    enc.setClickTimeout(500);       // Макс. время между кликами для двойного нажатия
    enc.setDebTimeout(50);          // Время гашения дребезга
    enc.setHoldTimeout(600);        // Время до срабатывания удержания (600 мс, как в тесте)
    enc.setStepTimeout(200);        // Период импульсов при удержании
    enc.setTimeout(1000);           // Общий таймаут действия
    
    // НАСТРОЙКА ЭНКОДЕРА
    enc.setEncReverse(0);           // 0 - нормальное направление
    enc.setEncType(EB_STEP4_LOW);   // Тип сигнала: 4 фазы за щелчок, активный LOW
    enc.setFastTimeout(30);          // Таймаут быстрого поворота
    
    // Сброс счётчика
    enc.counter = 0;
    
    // Таймер неактивности
    lastActivityTime = millis();
    
    Serial.println("[ENCODER] Энкодер инициализирован");
    Serial.printf("[ENCODER] Пины: CLK=%d, DT=%d, SW=%d\n", 
                  ENCODER_CLK, ENCODER_DT, ENCODER_SW);
    Serial.printf("[ENCODER] Таймаут удержания: 600 мс\n");
}

// ============================================================================
// ОПРОС ЭНКОДЕРА - ВЫЗЫВАТЬ КАЖДЫЕ 10-20 МС
// ============================================================================
void encoder_tick() {
    // Обязательно вызываем tick() для опроса библиотеки
    enc.tick();
    
    // Сброс таймера активности при любом действии
    if (enc.action()) {
        lastActivityTime = millis();
    }
    
    // ========================================================================
    // 1. ПОВОРОТ С УДЕРЖАНИЕМ (используем leftH/rightH как в тесте)
    // ========================================================================
    if (enc.leftH()) {
        Serial.println("[ENCODER] leftH -> HOLD_LEFT");
        EncoderEvent_t ev = EVENT_HOLD_LEFT;
        if (eventQueue != NULL) {
            xQueueSend(eventQueue, &ev, 0);
        }
    }
    
    if (enc.rightH()) {
        Serial.println("[ENCODER] rightH -> HOLD_RIGHT");
        EncoderEvent_t ev = EVENT_HOLD_RIGHT;
        if (eventQueue != NULL) {
            xQueueSend(eventQueue, &ev, 0);
        }
    }
    
    // ========================================================================
    // 2. ПОВОРОТ БЕЗ УДЕРЖАНИЯ
    // ========================================================================
    if (enc.left()) {
        Serial.println("[ENCODER] left -> LEFT");
        EncoderEvent_t ev = EVENT_ENCODER_LEFT;
        if (eventQueue != NULL) {
            xQueueSend(eventQueue, &ev, 0);
        }
    }
    
    if (enc.right()) {
        Serial.println("[ENCODER] right -> RIGHT");
        EncoderEvent_t ev = EVENT_ENCODER_RIGHT;
        if (eventQueue != NULL) {
            xQueueSend(eventQueue, &ev, 0);
        }
    }
    
    // ========================================================================
    // 3. КЛИКИ (одинарный и двойной)
    // ========================================================================
    if (enc.click()) {
        EncoderEvent_t ev;
        if (enc.hasClicks(2)) {
            ev = EVENT_BUTTON_DOUBLE;
            Serial.println("[ENCODER] double click");
        } else {
            ev = EVENT_BUTTON_CLICK;
            Serial.println("[ENCODER] click");
        }
        
        if (eventQueue != NULL) {
            xQueueSend(eventQueue, &ev, 0);
        }
    }
    
    // ========================================================================
    // 4. УДЕРЖАНИЕ БЕЗ ПОВОРОТА
    // ========================================================================
    if (enc.hold() && !enc.leftH() && !enc.rightH() && !enc.left() && !enc.right()) {
        Serial.println("[ENCODER] hold only -> HOLD");
        EncoderEvent_t ev = EVENT_BUTTON_HOLD;
        if (eventQueue != NULL) {
            xQueueSend(eventQueue, &ev, 0);
        }
    }
    
    // ========================================================================
    // 5. ОТПУСКАНИЕ ПОСЛЕ УДЕРЖАНИЯ (опционально, пока не используем)
    // ========================================================================
    // if (enc.release()) {
    //     Serial.println("[ENCODER] release");
    // }
    
    // ========================================================================
    // 6. ИМПУЛЬСНОЕ УДЕРЖАНИЕ (step) — пока не нужно
    // ========================================================================
    // if (enc.step()) {
    //     Serial.println("[ENCODER] step");
    // }
}

// ============================================================================
// СБРОС ТАЙМЕРА НЕАКТИВНОСТИ
// ============================================================================
void encoder_reset_inactivity_timer() {
    lastActivityTime = millis();
}

// ============================================================================
// ПОЛУЧЕНИЕ ВРЕМЕНИ НЕАКТИВНОСТИ
// ============================================================================
uint32_t encoder_get_inactivity_time() {
    return millis() - lastActivityTime;
}

// END ФАЙЛ: encoder_engine.cpp
