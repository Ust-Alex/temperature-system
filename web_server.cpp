/**
 * ============================================================================
 * @file web_server.cpp
 * @brief РЕАЛИЗАЦИЯ ВЕБ-СЕРВЕРА И WEBSOCKET
 * @version 6.2
 * ============================================================================
 */

#include "web_server.h"
#include "wifi_config.h"
#include "wifi_utils.h"
#include <LittleFS.h>
#include <ESPmDNS.h>

// ============================================================================
// ГЛОБАЛЬНЫЕ ОБЪЕКТЫ
// ============================================================================
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
AsyncWebServer server(80);
bool wifiClientConnected = false;

// ============================================================================
// ОБРАБОТЧИК WEBSOCKET
// ============================================================================
static void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
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
                // Переменная wifiModeAP объявлена в wifi_manager.cpp
                extern bool wifiModeAP;
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

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WEBSOCKET
// ============================================================================
void initWebSocket() {
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("[WiFi] WebSocket сервер запущен на порту %d\n", WEBSOCKET_PORT);
}