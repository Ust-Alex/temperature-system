/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket + WiFiManager + mDNS
 * @version 3.1
 * 
 * ОСОБЕННОСТИ:
 * - WiFiManager для подключения к роутеру (режим STA)
 * - mDNS для доступа по имени http://esp32ust.local
 * - Веб-сервер на порту 80 отдаёт index.html из LittleFS
 * - WebSocket на порту 8080 передаёт данные в реальном времени
 * - Кнопка BOOT (GPIO0) для сброса настроек WiFi
 * ============================================================================
 */

// ============================================================================
// БИБЛИОТЕКИ
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>          // ДЛЯ mDNS (доступ по имени)

// ============================================================================
// ПРОЕКТНЫЕ ЗАГОЛОВКИ
// ============================================================================
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

// Внешние переменные из других модулей
extern uint32_t timeStartMs;
extern bool timeIsCounting;

// ============================================================================
// ФОРМИРОВАНИЕ JSON С ДАННЫМИ ТЕМПЕРАТУР
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
  float t0 = sensors[0].temp;  // 100см
  float t1 = sensors[1].temp;  // 75см
  float t2 = sensors[2].temp;  // 50см
  float t3 = sensors[3].temp;  // гильза
  int mode = sysData.mode;
  int color = guildColorState;
  
  // Время в формате ЧЧ:ММ
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

// ============================================================================
// ОТПРАВКА ДАННЫХ КЛИЕНТАМ
// ============================================================================
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
      // Здесь можно добавить обработку команд от клиента
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
// ЗАДАЧА FREERTOS - УПРАВЛЕНИЕ WiFi, СЕРВЕРАМИ И mDNS
// ============================================================================
void taskWiFi(void* pvParameters) {
  Serial.println("[WiFi] Задача запущена (WiFiManager + mDNS)");

  // Настройка пина кнопки BOOT
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  // ==========================================================================
  // 1. НАСТРОЙКА WIFIMANAGER
  // ==========================================================================
  WiFiManager wm;
  
  wm.setConnectTimeout(30);        // 30 секунд на подключение
  wm.setConfigPortalTimeout(180);  // 3 минуты портал
  
  Serial.println("[WiFi] Попытка подключения к сохранённой сети...");
  
  // Пытаемся подключиться. Если не получается, запускается портал "TermoESP32"
  if (!wm.autoConnect("TermoESP32")) {
    Serial.println("[WiFi] ❌ Не удалось подключиться. Перезагрузка...");
    ESP.restart();
  }

  // ==========================================================================
  // 2. ПОДКЛЮЧЕНИЕ УСПЕШНО
  // ==========================================================================
  Serial.println("[WiFi] ✅ Подключено!");
  Serial.printf("[WiFi] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[WiFi] IP адрес: %s\n", WiFi.localIP().toString().c_str());

  // ==========================================================================
  // 3. ЗАПУСК mDNS (доступ по имени esp32ust.local)
  // ==========================================================================
  if (MDNS.begin("esp32ust")) {
    Serial.println("[mDNS] ✅ Запущен. Имя: http://esp32ust.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[mDNS] ❌ Ошибка запуска");
  }

  // ==========================================================================
  // 4. ЗАПУСК ВЕБ-СЕРВЕРА И WEBSOCKET
  // ==========================================================================
  initWebServer();
  initWebSocket();

  // ==========================================================================
  // 5. ОСНОВНОЙ ЦИКЛ ЗАДАЧИ
  // ==========================================================================
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastSend = 0;

  while (1) {
    // Обслуживаем WebSocket
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

    // Отправка данных всем клиентам раз в секунду
    if (now - lastSend >= 1000) {
      if (wifiClientConnected) {
        sendTemperaturesToClients();
      }
      lastSend = now;
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
  }
}