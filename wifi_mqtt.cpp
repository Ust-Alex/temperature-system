/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket для передачи данных
 * @version 2.2
 * 
 * ОСОБЕННОСТИ:
 * - Веб-сервер на порту 80 отдаёт index.html из LittleFS
 * - WebSocket на порту 8080 передаёт данные в реальном времени
 * - Добавлена передача времени с дисплея (ЧЧ:ММ)
 * - Добавлена передача базовой температуры гильзы (baseTemp)
 * ============================================================================
 */

#include "wifi_mqtt.h"
#include "globals.h"
#include "sensors.h"
#include "mode2_timer.h"  // Для mode2_timer_get_formatted()
#include "mode2_logic.h"  // Для guildBaseTemp
#include <LittleFS.h>

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
AsyncWebServer server(80);  // веб-сервер на порту 80
bool wifiClientConnected = false;

// Внешние переменные для времени из display_engine.cpp
extern uint32_t timeStartMs;
extern bool timeIsCounting;

// guildBaseTemp уже объявлена в mode2_logic.h

// ============================================================================
// ФОРМИРОВАНИЕ JSON С ТЕМПЕРАТУРАМИ И ВРЕМЕНЕМ
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
  float t0 = sensors[0].temp;  // 100см
  float t1 = sensors[1].temp;  // 75см
  float t2 = sensors[2].temp;  // 50см
  float t3 = sensors[3].temp;  // гильза
  int mode = sysData.mode;
  int color = guildColorState;
  
  // ========================================================================
  // ПОЛУЧАЕМ ТЕКУЩЕЕ ВРЕМЯ В ФОРМАТЕ ЧЧ:ММ
  // ========================================================================
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

  // ========================================================================
  // ПОЛУЧАЕМ БАЗОВУЮ ТЕМПЕРАТУРУ (правка №2)
  // ========================================================================
  float baseTemp = guildBaseTemp;  // из mode2_logic.cpp

  // ========================================================================
  // ФОРМИРУЕМ JSON (ДОБАВЛЕНО ПОЛЕ "baseTemp")
  // ========================================================================
  snprintf(buffer, bufferSize,
           "{"
           "\"guild\":%.2f,"
           "\"wall100\":%.2f,"
           "\"wall75\":%.2f,"
           "\"wall50\":%.2f,"
           "\"mode\":%d,"
           "\"color\":%d,"
           "\"time\":\"%s\","
           "\"baseTemp\":%.2f"          // новое поле
           "}",
           t3, t0, t1, t2, mode, color, timeStr, baseTemp);
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
  char jsonBuffer[160];  // чуть больше из-за baseTemp
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
        sendTemperaturesToClients();
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

  initWebServer();
  initWebSocket();

  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastSend = 0;

  while (1) {
    webSocket.loop();

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