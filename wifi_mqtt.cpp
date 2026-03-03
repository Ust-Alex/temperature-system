/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket + WiFiManager (режим STA)
 * @version 3.0
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "wifi_mqtt.h"
#include "globals.h"
#include "sensors.h"
#include "mode2_timer.h"
#include "mode2_logic.h"

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
AsyncWebServer server(80);
bool wifiClientConnected = false;

extern uint32_t timeStartMs;
extern bool timeIsCounting;

// ============================================================================
// ФОРМИРОВАНИЕ JSON (БЕЗ ИЗМЕНЕНИЙ)
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
  float t0 = sensors[0].temp;  // 100см
  float t1 = sensors[1].temp;  // 75см
  float t2 = sensors[2].temp;  // 50см
  float t3 = sensors[3].temp;  // гильза
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
           "\"guild\":%.2f,"
           "\"wall100\":%.2f,"
           "\"wall75\":%.2f,"
           "\"wall50\":%.2f,"
           "\"mode\":%d,"
           "\"color\":%d,"
           "\"time\":\"%s\","
           "\"baseTemp\":%.2f"
           "}",
           t3, t0, t1, t2, mode, color, timeStr, baseTemp);
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
// ОБРАБОТЧИК WEBSOCKET (БЕЗ ИЗМЕНЕНИЙ)
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
// ИНИЦИАЛИЗАЦИЯ ВЕБ-СЕРВЕРА (БЕЗ ИЗМЕНЕНИЙ)
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
// ИНИЦИАЛИЗАЦИЯ WEBSOCKET (БЕЗ ИЗМЕНЕНИЙ)
// ============================================================================
void initWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.printf("[WiFi] WebSocket сервер запущен на порту %d\n", WEBSOCKET_PORT);
}

// ============================================================================
// ЗАДАЧА FREERTOS (С ЛОГИКОЙ WIFIMANAGER)
// ============================================================================
void taskWiFi(void* pvParameters) {
  Serial.println("[WiFi] Задача запущена (WiFiManager)");

  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  // ==========================================================================
  // 1. Настройка WiFiManager
  // ==========================================================================
  WiFiManager wm;
  
  wm.setConnectTimeout(30);        // 30 секунд на подключение
  wm.setConfigPortalTimeout(180);  // 3 минуты портал
  
  // Пытаемся подключиться. Если не получается, запускается портал "TermoESP32" (без пароля)
  Serial.println("[WiFi] Попытка подключения к сохранённой сети...");
  
  if (!wm.autoConnect("TermoESP32")) {
    Serial.println("[WiFi] ❌ Не удалось подключиться. Перезагрузка...");
    ESP.restart();
  }

  // ==========================================================================
  // 2. Подключение успешно
  // ==========================================================================
  Serial.println("[WiFi] ✅ Подключено!");
  Serial.printf("[WiFi] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[WiFi] IP адрес: %s\n", WiFi.localIP().toString().c_str());

  // ==========================================================================
  // 3. Запуск веб-сервера и WebSocket
  // ==========================================================================
  initWebServer();
  initWebSocket();

  // ==========================================================================
  // 4. Основной цикл задачи
  // ==========================================================================
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastSend = 0;

  while (1) {
    webSocket.loop();

    // Проверка кнопки BOOT (сброс настроек при удержании 3 сек)
    static uint32_t lastButtonCheck = 0;
    uint32_t now = millis();
    
    if (now - lastButtonCheck > 100) {
      lastButtonCheck = now;
      
      if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
          Serial.println("[WiFi] Сброс настроек по кнопке");
          wm.resetSettings();
          ESP.restart();
        }
      }
    }

    // Отправка данных раз в секунду
    if (now - lastSend >= 1000) {
      if (wifiClientConnected) {
        sendTemperaturesToClients();
      }
      lastSend = now;
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
  }
}