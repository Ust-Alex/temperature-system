/**
 * ============================================================================
 * @file encoder_engine.cpp
 * @brief РЕАЛИЗАЦИЯ РАБОТЫ С ЭНКОДЕРОМ KY-040
 * 
 * @version 4.2
 * @date 2026
 * 
 * @details ОСОБЕННОСТИ:
 *          - Использует библиотеку EncButton для надёжной обработки
 *          - Поддерживает: поворот (left/right), поворот с удержанием (leftH/rightH),
 *            клик, двойной клик, удержание без поворота
 *          - Отправляет события в очередь FreeRTOS
 *          - Встроенный таймер неактивности пользователя
 *          - Подробная отладка всех событий
 * ============================================================================
 */

#include "encoder_engine.h"
#include "system_config.h"
#include <EncButton.h>

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

/**
 * @brief Объект энкодера из библиотеки EncButton
 * @param ENCODER_CLK Пин CLK
 * @param ENCODER_DT Пин DT
 * @param ENCODER_SW Пин кнопки
 * @param INPUT Режим пинов энкодера (внешняя подтяжка)
 * @param INPUT_PULLUP Режим пина кнопки (внутренняя подтяжка)
 */
EncButton enc(ENCODER_CLK, ENCODER_DT, ENCODER_SW, INPUT, INPUT_PULLUP);

/** @brief Время последней активности пользователя (для таймаута) */
static uint32_t lastActivityTime = 0;

/** @brief Очередь событий FreeRTOS (внешняя, объявлена в globals.h) */
extern QueueHandle_t eventQueue;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРА
// ============================================================================

void encoder_init() {
    /**
     * @brief Настраивает энкодер и таймауты
     * @note Должна вызываться один раз при старте системы
     */
    Serial.println("[ENCODER] Инициализация энкодера...");
    
    // Настройка уровня сигнала кнопки (LOW - замыкание на GND)
    enc.setBtnLevel(LOW);
    
    // Настройка таймаутов (в миллисекундах)
    enc.setClickTimeout(500);   // Макс. время между кликами для двойного нажатия
    enc.setDebTimeout(50);       // Время гашения дребезга контактов
    enc.setHoldTimeout(600);     // Время до срабатывания удержания (600 мс)
    enc.setStepTimeout(200);     // Период импульсов при удержании
    enc.setTimeout(1000);        // Общий таймаут действия
    
    // Настройка энкодера
    enc.setEncReverse(0);        // 0 - нормальное направление, 1 - реверс
    enc.setEncType(EB_STEP4_LOW); // Тип сигнала: 4 фазы за щелчок, активный LOW
    enc.setFastTimeout(30);       // Таймаут быстрого поворота
    
    // Сброс счётчика энкодера
    enc.counter = 0;
    
    // Инициализация таймера неактивности
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
    /**
     * @brief Опрашивает энкодер и отправляет события в очередь
     * @note Должна вызываться регулярно из задачи (например, каждые 10-20 мс)
     */
    enc.tick();
    
    // Сброс таймера активности при любом действии
    if (enc.action()) {
        lastActivityTime = millis();
    }
    
    // ========================================================================
    // 1. ПОВОРОТ С УДЕРЖАНИЕМ (приоритет: сначала обрабатываем удержание)
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
    // 5. ОТПУСКАНИЕ ПОСЛЕ УДЕРЖАНИЯ (опционально, закомментировано)
    // ========================================================================
    // if (enc.release()) {
    //     Serial.println("[ENCODER] release");
    //     // Можно добавить событие, если нужно
    // }
    
    // ========================================================================
    // 6. ИМПУЛЬСНОЕ УДЕРЖАНИЕ (step) — пока не используется
    // ========================================================================
    // if (enc.step()) {
    //     Serial.println("[ENCODER] step");
    // }
}

// ============================================================================
// УПРАВЛЕНИЕ ТАЙМЕРОМ НЕАКТИВНОСТИ
// ============================================================================

void encoder_reset_inactivity_timer() {
    /** @brief Принудительный сброс таймера неактивности */
    lastActivityTime = millis();
}

uint32_t encoder_get_inactivity_time() {
    /** @return Время в миллисекундах с последнего действия пользователя */
    return millis() - lastActivityTime;
}