/**
 * ============================================================================
 * @file task_wifi.cpp
 * @brief ЗАДАЧА FREERTOS ДЛЯ УПРАВЛЕНИЯ Wi-Fi
 * @version 6.2
 * 
 * Основной цикл:
 * 1. Подключение к роутеру (или переход в AP)
 * 2. Запуск веб-сервера и WebSocket
 * 3. Периодическая отправка данных клиентам
 * 4. Контроль потери связи и переподключение
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "wifi_config.h"
#include "wifi_manager.h"
#include "wifi_utils.h"
#include "web_server.h"

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern bool wifiModeAP;

// ============================================================================
// ЗАДАЧА FREERTOS
// ============================================================================
void taskWiFi(void* pvParameters) {
    Serial.println("[WiFi] Задача запущена (гибридный режим AP/STA)");
    
    // ========================================================================
    // 1. ПЕРВИЧНОЕ ПОДКЛЮЧЕНИЕ (ПРИ ЗАГРУЗКЕ)
    // ========================================================================
    bool connected = connectSTA();
    
    if (connected) {
        // Запускаем mDNS
        if (MDNS.begin("esp32ust")) {
            Serial.println("[mDNS] ✅ Запущен. Имя: http://esp32ust.local");
            MDNS.addService("http", "tcp", 80);
        } else {
            Serial.println("[mDNS] ❌ Ошибка запуска");
        }
    } else {
        Serial.println("[WiFi] Переключение в режим AP...");
        delay(2000);
        startAP();
    }
    
    // ========================================================================
    // 2. ЗАПУСК ВЕБ-СЕРВЕРА И WEBSOCKET
    // ========================================================================
    initWebServer();
    initWebSocket();
    
    // ========================================================================
    // 3. ОСНОВНОЙ ЦИКЛ
    // ========================================================================
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t lastSend = 0;
    uint32_t lastReconnectAttempt = 0;
    
    while (1) {
        webSocket.loop();
        
        // ---- ОТПРАВКА ДАННЫХ КЛИЕНТАМ (раз в секунду) ----
        uint32_t now = millis();
        if (now - lastSend >= 1000) {
            if (wifiClientConnected) {
                sendTemperaturesToClients();
            }
            lastSend = now;
        }
        
        // ---- КОНТРОЛЬ ПОТЕРИ СВЯЗИ (только в режиме STA) ----
        if (!wifiModeAP) {
            if (WiFi.status() != WL_CONNECTED) {
                // Проверяем статус раз в 30 секунд
                if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
                    lastReconnectAttempt = now;
                    Serial.println("[WiFi] ⚠️ Связь с роутером потеряна!");
                    
                    bool reconnected = reconnectSTA();
                    
                    if (reconnected) {
                        // Перезапускаем mDNS (на случай смены IP)
                        MDNS.end();
                        if (MDNS.begin("esp32ust")) {
                            Serial.println("[mDNS] ✅ Перезапущен. Имя: http://esp32ust.local");
                            MDNS.addService("http", "tcp", 80);
                        }
                    } else {
                        Serial.println("[WiFi] ❌ Не удалось восстановить связь. Переключение в AP...");
                        delay(2000);
                        startAP();
                    }
                }
            } else {
                // Связь есть — сбрасываем таймер последней проверки
                lastReconnectAttempt = now;
            }
        }
        
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
    }
}