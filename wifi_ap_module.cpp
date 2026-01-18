#include <WiFi.h>
#include <WebServer.h>
#include "wifi_ap_module.h"
#include "system_config.h"
#include <WebSocketsServer.h>

// ========== КОНФИГУРАЦИЯ ==========
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID      "Temperature_System"
#endif

#ifndef WIFI_AP_PASSWORD  
#define WIFI_AP_PASSWORD  "termo1234"
#endif

#ifndef WIFI_AP_CHANNEL
#define WIFI_AP_CHANNEL   6
#endif

#ifndef WEB_SERVER_PORT
#define WEB_SERVER_PORT   80
#endif

#ifndef WS_SERVER_PORT
#define WS_SERVER_PORT    81
#endif

#ifndef WEB_UPDATE_INTERVAL_MS
#define WEB_UPDATE_INTERVAL_MS 500
#endif

// ========== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==========
WebServer server(WEB_SERVER_PORT);
WebSocketsServer webSocket(WS_SERVER_PORT);
uint32_t lastWebUpdate = 0;
bool wifiAPStarted = false;

// ========== HTML ВЕБ-СТРАНИЦЫ (сокращенная версия) ==========
const char WEB_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Температурная система</title>
    <style>
        body { font-family: Arial; background: #2c3e50; color: white; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 30px; }
        .sensor-card { background: rgba(255,255,255,0.1); border-radius: 10px; padding: 15px; margin: 10px 0; }
        .sensor-name { font-size: 1.2rem; font-weight: bold; margin-bottom: 5px; }
        .temperature { font-size: 2.5rem; font-weight: bold; font-family: monospace; }
        .status { background: #27ae60; padding: 5px 10px; border-radius: 5px; display: inline-block; margin-top: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📡 Температурная система</h1>
            <p>Мониторинг в реальном времени</p>
        </div>
        <div id="sensors-container">
            <!-- Данные появятся здесь -->
        </div>
        <div class="status">WebSocket: <span id="ws-status">Подключение...</span></div>
    </div>

    <script>
        const SENSOR_NAMES = ["Стенка 100см", "Стенка 75см", "Стенка 50см", "Гильза 25см"];
        
        function initializeSensors() {
            const container = document.getElementById('sensors-container');
            container.innerHTML = '';
            for(let i = 0; i < 4; i++) {
                const card = document.createElement('div');
                card.className = 'sensor-card';
                card.innerHTML = `
                    <div class="sensor-name">${SENSOR_NAMES[i]}</div>
                    <div class="temperature" id="temp-${i}">--.-- °C</div>
                    <div id="time-${i}">Ожидание данных...</div>
                `;
                container.appendChild(card);
            }
        }
        
        let ws;
        function connectWebSocket() {
            const wsUrl = 'ws://' + location.hostname + ':81/';
            ws = new WebSocket(wsUrl);
            
            ws.onopen = () => {
                document.getElementById('ws-status').textContent = '✅ Подключено';
                document.getElementById('ws-status').style.color = '#27ae60';
            };
            
            ws.onclose = () => {
                document.getElementById('ws-status').textContent = '❌ Отключено';
                document.getElementById('ws-status').style.color = '#e74c3c';
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    for(let i = 0; i < 4; i++) {
                        const temp = data['t' + i];
                        const element = document.getElementById('temp-' + i);
                        if(element && temp !== null && temp !== undefined) {
                            element.textContent = parseFloat(temp).toFixed(2) + ' °C';
                            document.getElementById('time-' + i).textContent = 
                                'Обновлено: ' + new Date().toLocaleTimeString();
                        }
                    }
                } catch(e) {
                    console.error('Ошибка:', e);
                }
            };
        }
        
        document.addEventListener('DOMContentLoaded', () => {
            initializeSensors();
            connectWebSocket();
        });
    </script>
</body>
</html>
)rawliteral";

// ========== ОБРАБОТЧИКИ WEB SOCKET ==========
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WebSocket] Клиент %u отключен\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WebSocket] Клиент %u подключен\n", num);
            wifi_ap_send_temperatures();
            break;
        }
    }
}

// ========== ОСНОВНЫЕ ФУНКЦИИ ==========
void wifi_ap_setup() {
    Serial.println("\n=== ИНИЦИАЛИЗАЦИЯ WI-FI AP ===");
    
    WiFi.mode(WIFI_AP);
    
    if(!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL)) {
        Serial.println("[WiFi] Ошибка запуска AP!");
        return;
    }
    
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", WEB_PAGE_HTML);
    });
    
    server.onNotFound([]() {
        server.send(404, "text/plain", "Страница не найдена");
    });
    
    server.begin();
    webSocket.onEvent(webSocketEvent);
    webSocket.begin();
    wifiAPStarted = true;
    
    Serial.println("=================================");
    Serial.print("Wi-Fi AP: ");
    Serial.println(WIFI_AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("=================================\n");
}

void wifi_ap_loop() {
    if(!wifiAPStarted) return;
    
    server.handleClient();
    webSocket.loop();
    
    uint32_t currentTime = millis();
    if(currentTime - lastWebUpdate >= WEB_UPDATE_INTERVAL_MS) {
        lastWebUpdate = currentTime;
        wifi_ap_send_temperatures();
    }
}

void wifi_ap_send_temperatures() {
    if(!wifiAPStarted || webSocket.connectedClients() == 0) return;
    
    if(xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        String json = "{";
        for(int i = 0; i < 4; i++) {
            float temp = sensors[i].temp;
            if(temp <= TEMP_NO_DATA + 0.1f) {
                json += "\"t" + String(i) + "\":null";
            } else {
                json += "\"t" + String(i) + "\":" + String(temp, 2);
            }
            if(i < 3) json += ",";
        }
        json += "}";
        
        webSocket.broadcastTXT(json.c_str(), json.length());
        xSemaphoreGive(dataMutex);
    }
}