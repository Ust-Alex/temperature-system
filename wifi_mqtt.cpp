/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket + mDNS + ГИБРИДНЫЙ РЕЖИМ AP/STA
 * @version 6.1 (ДВЕ ЖЁСТКО ЗАДАННЫЕ СЕТИ + СТАТИЧЕСКИЙ IP ДЛЯ КАЖДОЙ)
 * 
 * ЛОГИКА РАБОТЫ:
 * 1. При загрузке ESP32 пытается подключиться к ПЕРВОЙ сети (MY_SSID_1).
 *    - Использует статический IP: 192.168.1.100
 *    - Делает 3 попытки по 5 секунд каждая.
 *    - Если успешно → запоминает эту сеть как "последняя успешная".
 * 2. Если первая сеть не подошла → пытается подключиться ко ВТОРОЙ сети (MY_SSID_2).
 *    - Использует статический IP: 192.168.100.100
 *    - Делает 3 попытки по 5 секунд каждая.
 *    - Если успешно → запоминает эту сеть как "последняя успешная".
 * 3. Если ни одна сеть не подошла → переключается в режим AP (точка доступа).
 * 4. При потере связи в режиме STA:
 *    - Сначала пытается переподключиться к "последней успешной" сети (3 попытки).
 *    - Если не получилось → перебирает все сети по порядку (сначала первую, потом вторую).
 *    - Если ни одна не подошла → переключается в режим AP.
 * 5. EEPROM НЕ ИСПОЛЬЗУЕТСЯ. Все настройки только в коде.
 * 6. В лог выводится РЕАЛЬНАЯ мощность передатчика (чтение из чипа).
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>

#include "wifi_mqtt.h"
#include "globals.h"
#include "sensors.h"
#include "mode2_timer.h"
#include "mode2_logic.h"
#include "mp3_player.h"

// ============================================================================
// КОНФИГУРАЦИЯ Wi-Fi (НАСТРАИВАЕТСЯ ПОЛЬЗОВАТЕЛЕМ)
// ============================================================================

// ----- ДВЕ ЖЁСТКО ЗАДАННЫЕ СЕТИ (для подключения к роутеру) -----
#define MY_SSID_1 "cherven30_TP-Link"   // Первая сеть (SSID)
#define MY_PASS_1 "Bb_00010121"        // Пароль от первой сети

#define MY_SSID_2 "Xiaomi_UST"          // Вторая сеть (SSID)
#define MY_PASS_2 "1456654123"        // Пароль от второй сети

// ----- СТАТИЧЕСКИЕ IP ДЛЯ КАЖДОЙ СЕТИ -----
// Для первой сети (cherven30_TP-Link, 192.168.1.x)
#define STA1_IP 192, 168, 1, 100        // IP для ESP32 в первой сети
#define STA1_GATEWAY 192, 168, 1, 1     // Шлюз (роутер)
#define STA1_SUBNET 255, 255, 255, 0    // Маска подсети

// Для второй сети (Xiaomi_UST, 192.168.100.x)
#define STA2_IP 192, 168, 100, 100      // IP для ESP32 во второй сети
#define STA2_GATEWAY 192, 168, 100, 1   // Шлюз (роутер)
#define STA2_SUBNET 255, 255, 255, 0    // Маска подсети

// ----- ПАРАМЕТРЫ РЕТРАЕВ (активных попыток подключения) -----
#define STA_RETRY_ATTEMPTS 3            // Количество попыток на одну сеть
#define STA_ATTEMPT_TIMEOUT_MS 5000     // Таймаут одной попытки (мс)
#define STA_RETRY_DELAY_MS 1000         // Пауза между попытками (мс)

// ----- НАСТРОЙКИ РЕЖИМА AP (точки доступа) -----
#define AP_SSID "TermoESP32"            // Имя точки доступа
#define AP_PASSWORD ""                  // Пароль (пустой = открытая сеть)
#define AP_CHANNEL 6                    // Канал Wi-Fi
#define AP_IP 192, 168, 4, 1            // IP-адрес точки доступа
#define AP_GATEWAY 192, 168, 4, 1       // Шлюз
#define AP_SUBNET 255, 255, 255, 0      // Маска подсети
#define AP_TX_POWER 80                  // 80 = 20 дБм (максимальная мощность)

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
AsyncWebServer server(80);
bool wifiClientConnected = false;
bool wifiModeAP = true;  // true = AP, false = STA

// Запоминаем последнюю успешную сеть для переподключения
String lastConnectedSSID = "";

extern uint32_t timeStartMs;
extern bool timeIsCounting;

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: ПОЛУЧЕНИЕ РЕАЛЬНОЙ МОЩНОСТИ
// ============================================================================
int8_t getRealTxPower() {
    int8_t power = 0;
    if (esp_wifi_get_max_tx_power(&power) == ESP_OK) {
        return power / 4;  // Преобразуем в дБм
    }
    return -1;  // Ошибка
}

// ============================================================================
// ФУНКЦИЯ ПОДКЛЮЧЕНИЯ К ОДНОЙ СЕТИ С РЕТРАЯМИ И СТАТИЧЕСКИМ IP
// ============================================================================
bool connectToNetwork(const char* ssid, const char* password, 
                      IPAddress localIP, IPAddress gateway, IPAddress subnet, 
                      bool isLastAttempt) {
    Serial.printf("[WiFi] Сеть %s: попытка 1/%d...\n", ssid, STA_RETRY_ATTEMPTS);
    
    for (int attempt = 1; attempt <= STA_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 1) {
            Serial.printf("[WiFi] Сеть %s: попытка %d/%d...\n", ssid, attempt, STA_RETRY_ATTEMPTS);
        }
        
        // ============================================================
        // СБРОС Wi-Fi ПЕРЕД КАЖДОЙ ПОПЫТКОЙ
        // ============================================================
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_STA);
        
        // ============================================================
        // УСТАНОВКА СТАТИЧЕСКОГО IP ДЛЯ ЭТОЙ СЕТИ
        // ============================================================
        if (!WiFi.config(localIP, gateway, subnet)) {
            Serial.println("[WiFi] ⚠️ Не удалось настроить статический IP");
        } else {
            Serial.printf("[WiFi] Статический IP установлен: %d.%d.%d.%d\n", 
                          localIP[0], localIP[1], localIP[2], localIP[3]);
        }
        
        WiFi.begin(ssid, password);
        
        uint32_t startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < STA_ATTEMPT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            lastConnectedSSID = String(ssid);
            int8_t realPower = getRealTxPower();
            if (realPower >= 0) {
                Serial.printf("[WiFi] ✅ Подключено к %s. IP: %s, мощность: %d дБм\n", 
                              ssid, WiFi.localIP().toString().c_str(), realPower);
            } else {
                Serial.printf("[WiFi] ✅ Подключено к %s. IP: %s, мощность: ОШИБКА\n", 
                              ssid, WiFi.localIP().toString().c_str());
            }
            return true;
        }
        
        if (!(attempt == STA_RETRY_ATTEMPTS && isLastAttempt)) {
            delay(STA_RETRY_DELAY_MS);
        }
    }
    
    Serial.printf("[WiFi] ❌ Не удалось подключиться к %s\n", ssid);
    return false;
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ К РОУТЕРУ (ПЕРЕБОР ДВУХ СЕТЕЙ С РЕТРАЯМИ)
// ============================================================================
bool connectSTA() {
    Serial.println("[WiFi] === НАЧАЛО ПОДКЛЮЧЕНИЯ К РОУТЕРУ ===");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    
    // Установка максимальной мощности (один раз)
    esp_wifi_set_max_tx_power(AP_TX_POWER);
    
    // ----- ПЫТАЕМСЯ ПОДКЛЮЧИТЬСЯ К ПЕРВОЙ СЕТИ (cherven30_TP-Link) -----
    IPAddress ip1(STA1_IP);
    IPAddress gw1(STA1_GATEWAY);
    IPAddress sn1(STA1_SUBNET);
    
    bool connected = connectToNetwork(MY_SSID_1, MY_PASS_1, ip1, gw1, sn1, false);
    
    // ----- ЕСЛИ НЕ ПОЛУЧИЛОСЬ — ПРОБУЕМ ВТОРУЮ СЕТЬ (Xiaomi_UST) -----
    if (!connected) {
        Serial.printf("[WiFi] Пробую вторую сеть: %s...\n", MY_SSID_2);
        
        IPAddress ip2(STA2_IP);
        IPAddress gw2(STA2_GATEWAY);
        IPAddress sn2(STA2_SUBNET);
        
        connected = connectToNetwork(MY_SSID_2, MY_PASS_2, ip2, gw2, sn2, true);
    }
    
    // ----- РЕЗУЛЬТАТ -----
    if (connected) {
        wifiModeAP = false;
        Serial.printf("[WiFi] === УСПЕШНО ПОДКЛЮЧЕНО к %s ===\n", lastConnectedSSID.c_str());
        return true;
    } else {
        wifiModeAP = true;
        Serial.println("[WiFi] === НЕ УДАЛОСЬ ПОДКЛЮЧИТЬСЯ НИ К ОДНОЙ СЕТИ ===");
        return false;
    }
}

// ============================================================================
// ПЕРЕПОДКЛЮЧЕНИЕ ПРИ ПОТЕРЕ СВЯЗИ
// ============================================================================
bool reconnectSTA() {
    Serial.println("[WiFi] === ПОПЫТКА ВОССТАНОВЛЕНИЯ СВЯЗИ ===");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_max_tx_power(AP_TX_POWER);
    
    // ----- 1. СНАЧАЛА ПРОБУЕМ ПОСЛЕДНЮЮ УСПЕШНУЮ СЕТЬ -----
    if (lastConnectedSSID.length() > 0) {
        Serial.printf("[WiFi] Пробую последнюю успешную сеть: %s...\n", lastConnectedSSID.c_str());
        
        // Определяем IP-настройки для этой сети
        IPAddress ip, gw, sn;
        const char* pass = "";
        
        if (lastConnectedSSID == MY_SSID_1) {
            ip = IPAddress(STA1_IP);
            gw = IPAddress(STA1_GATEWAY);
            sn = IPAddress(STA1_SUBNET);
            pass = MY_PASS_1;
        } else if (lastConnectedSSID == MY_SSID_2) {
            ip = IPAddress(STA2_IP);
            gw = IPAddress(STA2_GATEWAY);
            sn = IPAddress(STA2_SUBNET);
            pass = MY_PASS_2;
        } else {
            Serial.printf("[WiFi] ⚠️ Неизвестная сеть: %s\n", lastConnectedSSID.c_str());
            return false;
        }
        
        bool connected = connectToNetwork(lastConnectedSSID.c_str(), pass, ip, gw, sn, false);
        if (connected) {
            wifiModeAP = false;
            Serial.printf("[WiFi] === СВЯЗЬ ВОССТАНОВЛЕНА с %s ===\n", lastConnectedSSID.c_str());
            return true;
        }
    }
    
    // ----- 2. ЕСЛИ НЕ ПОЛУЧИЛОСЬ — ПЕРЕБИРАЕМ ВСЕ СЕТИ ПО ПОРЯДКУ -----
    Serial.println("[WiFi] Перебираю все сети по порядку...");
    
    IPAddress ip1(STA1_IP);
    IPAddress gw1(STA1_GATEWAY);
    IPAddress sn1(STA1_SUBNET);
    
    bool connected = connectToNetwork(MY_SSID_1, MY_PASS_1, ip1, gw1, sn1, false);
    
    if (!connected) {
        IPAddress ip2(STA2_IP);
        IPAddress gw2(STA2_GATEWAY);
        IPAddress sn2(STA2_SUBNET);
        connected = connectToNetwork(MY_SSID_2, MY_PASS_2, ip2, gw2, sn2, true);
    }
    
    if (connected) {
        wifiModeAP = false;
        Serial.printf("[WiFi] === СВЯЗЬ ВОССТАНОВЛЕНА с %s ===\n", lastConnectedSSID.c_str());
        return true;
    } else {
        wifiModeAP = true;
        Serial.println("[WiFi] === ВОССТАНОВИТЬ СВЯЗЬ НЕ УДАЛОСЬ ===");
        return false;
    }
}

// ============================================================================
// ЗАПУСК РЕЖИМА AP (ТОЧКА ДОСТУПА)
// ============================================================================
void startAP() {
    Serial.println("[WiFi] Запуск режима AP...");
    IPAddress localIP(AP_IP);
    IPAddress gateway(AP_GATEWAY);
    IPAddress subnet(AP_SUBNET);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(localIP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
    WiFi.setSleep(false);
    
    delay(200);
    esp_wifi_set_max_tx_power(AP_TX_POWER);
    
    wifiModeAP = true;
    lastConnectedSSID = "";  // Сбрасываем запомненную сеть
    
    // Получаем реальную мощность
    int8_t realPower = getRealTxPower();
    if (realPower >= 0) {
        Serial.printf("[WiFi] AP запущена. SSID: %s, IP: 192.168.4.1, канал: %d, мощность: %d дБм\n", 
                      AP_SSID, AP_CHANNEL, realPower);
    } else {
        Serial.printf("[WiFi] AP запущена. SSID: %s, IP: 192.168.4.1, канал: %d, мощность: ОШИБКА\n", 
                      AP_SSID, AP_CHANNEL);
    }
}

// ============================================================================
// ОТПРАВКА ТЕМПЕРАТУРНЫХ ДАННЫХ КЛИЕНТАМ
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
    float temps[6];
    for (int i = 0; i < 6; i++) {
        temps[i] = sensors[i].found ? sensors[i].temp : 0.0f;
    }
    
    int mode = sysData.mode;
    int color = guildColorState;
    
    char timeStr[6] = "00:00";
    if (mode == 0) {
        if (timeIsCounting) {
            uint32_t elapsed = millis() - timeStartMs;
            uint32_t minutes = elapsed / 60000UL;
            uint8_t hours = minutes / 60;
            uint8_t mins = minutes % 60;
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hours, mins);
        }
    } else {
        String mode2time = mode2_timer_get_formatted();
        strncpy(timeStr, mode2time.c_str(), sizeof(timeStr) - 1);
        timeStr[5] = '\0';
    }
    
    float baseTemp = guildBaseTemp;
    
    snprintf(buffer, bufferSize,
             "{"
             "\"temps\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
             "\"mode\":%d,"
             "\"color\":%d,"
             "\"time\":\"%s\","
             "\"baseTemp\":%.2f"
             "}",
             temps[0], temps[1], temps[2], temps[3], temps[4], temps[5],
             mode, color, timeStr, baseTemp);
}

void broadcastJSON(const char* json) {
    if (wifiClientConnected) {
        webSocket.broadcastTXT(json);
    }
}

void sendTemperaturesToClients() {
    char jsonBuffer[160];
    buildTemperaturesJSON(jsonBuffer, sizeof(jsonBuffer));
    broadcastJSON(jsonBuffer);
}

// ============================================================================
// ОТПРАВКА ЗВУКОВЫХ КОМАНД КЛИЕНТАМ
// ============================================================================
void sendSoundCommand(const char* soundType) {
    if (!wifiClientConnected) return;
    
    char soundMsg[64];
    snprintf(soundMsg, sizeof(soundMsg), "{\"sound\":\"%s\"}", soundType);
    webSocket.broadcastTXT(soundMsg);
    Serial.printf("[WiFi] Отправлен звук: %s\n", soundType);
}

// ============================================================================
// ОБРАБОТЧИК WEBSOCKET
// ============================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WiFi] Клиент #%u отключился\n", num);
            wifiClientConnected = false;
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WiFi] Клиент #%u подключился: %s\n", num, ip.toString().c_str());
            wifiClientConnected = true;
            sendTemperaturesToClients();
            break;
        }
        
        case WStype_TEXT: {
            String msg = String((char*)payload);
            
            // Команда: WIFI_SET:ssid:pass
            if (msg.startsWith("WIFI_SET:")) {
                String data = msg.substring(9);
                int sep = data.indexOf(':');
                if (sep > 0) {
                    String ssid = data.substring(0, sep);
                    String pass = data.substring(sep + 1);
                    // TODO: сохранить в EEPROM (пока не используется)
                    webSocket.broadcastTXT("{\"status\":\"saved, rebooting...\"}");
                    delay(1000);
                    ESP.restart();
                }
            }
            // Команда: WIFI_AP (переключиться в AP)
            else if (msg == "WIFI_AP") {
                webSocket.broadcastTXT("{\"status\":\"switching to AP...\"}");
                delay(1000);
                ESP.restart();
            }
            // Команда: WIFI_STATUS (запрос статуса)
            else if (msg == "WIFI_STATUS") {
                String status = "{";
                status += "\"mode\":\"" + String(wifiModeAP ? "AP" : "STA") + "\",";
                status += "\"ssid\":\"" + (wifiModeAP ? AP_SSID : WiFi.SSID()) + "\",";
                status += "\"ip\":\"" + (wifiModeAP ? "192.168.4.1" : WiFi.localIP().toString()) + "\"";
                status += "}";
                webSocket.broadcastTXT(status);
            }
            break;
        }
        
        default:
            break;
    }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ВЕБ-СЕРВЕРА
// ============================================================================
void initWebServer() {
    if (!LittleFS.begin()) {
        Serial.println("[WEB] ❌ Ошибка монтирования LittleFS!");
        return;
    }
    Serial.println("[WEB] ✅ LittleFS смонтирована");
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    server.serveStatic("/", LittleFS, "/");
    server.begin();
    Serial.println("[WEB] ✅ Сервер запущен на порту 80");
}

void initWebSocket() {
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("[WiFi] WebSocket сервер запущен на порту %d\n", WEBSOCKET_PORT);
}

// ============================================================================
// ЗАДАЧА FREERTOS - УПРАВЛЕНИЕ WiFi, СЕРВЕРАМИ И mDNS
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
    const uint32_t RECONNECT_INTERVAL_MS = 30000;  // Проверка каждые 30 сек
    
    while (1) {
        webSocket.loop();
        
        // Отправка данных клиентам (раз в секунду)
        uint32_t now = millis();
        if (now - lastSend >= 1000) {
            if (wifiClientConnected) {
                sendTemperaturesToClients();
            }
            lastSend = now;
        }
        
        // ====================================================================
        // КОНТРОЛЬ ПОТЕРИ СВЯЗИ (только в режиме STA)
        // ====================================================================
        if (!wifiModeAP) {
            if (WiFi.status() != WL_CONNECTED) {
                // Проверяем статус раз в 30 секунд
                if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
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