/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket для передачи данных
 * @version 2.0
 * 
 * ОСОБЕННОСТИ:
 * - Веб-сервер на порту 80 отдаёт index.html из LittleFS
 * - WebSocket на порту 8080 передаёт данные в реальном времени
 * ============================================================================
 */

#include "wifi_mqtt.h"
#include "globals.h"
#include "sensors.h"
#include <LittleFS.h>

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
AsyncWebServer server(80);  // веб-сервер на порту 80
bool wifiClientConnected = false;

// ============================================================================
// ФОРМИРОВАНИЕ JSON С ТЕМПЕРАТУРАМИ
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
  float t0 = sensors[0].temp;
  float t1 = sensors[1].temp;
  float t2 = sensors[2].temp;
  float t3 = sensors[3].temp;
  int mode = sysData.mode;
  int color = guildColorState;

  snprintf(buffer, bufferSize,
           "{"
           "\"guild\":%.2f,"
           "\"wall100\":%.2f,"
           "\"wall75\":%.2f,"
           "\"wall50\":%.2f,"
           "\"mode\":%d,"
           "\"color\":%d"
           "}",
           t3, t0, t1, t2, mode, color);
}

// ============================================================================
// ОТПРАВКА ДАННЫХ КЛИЕНТАМ
// ============================================================================
void broadcastJSON(const char* json) {
  if (wifiClientConnected) {
    webSocket.broadcastTXT(json);
  }
}

void sendTemperaturesToClients() {
  char jsonBuffer[128];
  buildTemperaturesJSON(jsonBuffer, sizeof(jsonBuffer));
  broadcastJSON(jsonBuffer);
}

// ============================================================================
// ОБРАБОТЧИК СОБЫТИЙ WEBSOCKET
// ============================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WiFi] Клиент #%u отключился\n", num);
      wifiClientConnected = false;
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WiFi] Клиент #%u подключился: %s\n", num, ip.toString().c_str());
        wifiClientConnected = true;
        sendTemperaturesToClients();  // сразу отправляем данные
      }
      break;

    case WStype_TEXT:
      // можно добавить обработку команд позже
      break;

    default:
      break;
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ВЕБ-СЕРВЕРА (отдаёт index.html из LittleFS)
// ============================================================================
void initWebServer() {
  // Монтируем файловую систему LittleFS
  if (!LittleFS.begin()) {
    Serial.println("[WEB] ❌ Ошибка монтирования LittleFS!");
    return;
  }
  Serial.println("[WEB] ✅ LittleFS смонтирована");

  // При обращении к корню сайта отдаём index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // +++ ЭТА СТРОКА РАЗРЕШАЕТ ОТДАЧУ ЛЮБЫХ ФАЙЛОВ +++
  server.serveStatic("/", LittleFS, "/");

  // Запускаем сервер
  server.begin();
  Serial.println("[WEB] ✅ Сервер запущен на порту 80");
  Serial.println("[WEB] Откройте в браузере http://192.168.4.1");
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WEBSOCKET
// ============================================================================
void initWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.printf("[WiFi] WebSocket сервер запущен на порту %d\n", WEBSOCKET_PORT);
}

// ============================================================================
// ЗАДАЧА FREERTOS
// ============================================================================
void taskWiFi(void* pvParameters) {
  Serial.println("[WiFi] Задача запущена");

  initWebServer();  // запускаем веб-сервер (порт 80)
  initWebSocket();  // запускаем WebSocket (порт 8080)

  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastSend = 0;

  while (1) {
    webSocket.loop();  // обслуживаем WebSocket

    // Раз в секунду отправляем данные всем клиентам
    uint32_t now = millis();
    if (now - lastSend >= 1000) {
      if (wifiClientConnected) {
        sendTemperaturesToClients();
      }
      lastSend = now;
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
  }
}