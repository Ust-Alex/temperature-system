/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket + mDNS + ГИБРИДНЫЙ РЕЖИМ AP/STA
 * @version 5.1 (УВЕЛИЧЕНА МОЩНОСТЬ AP ДО 20 дБм)
 * 
 * ЛОГИКА РАБОТЫ:
 * 1. При загрузке ESP32 проверяет EEPROM: есть ли сохранённые SSID и пароль?
 * 2. Если есть — пытается подключиться к роутеру (режим STA) в течение 15 секунд.
 * 3. Если подключение успешно — работает в STA, точка доступа ВЫКЛЮЧЕНА.
 * 4. Если подключение не удалось (нет сети, неверный пароль) — через 15 секунд
 *    переключается в режим AP (точка доступа TermoESP32, IP 192.168.4.1).
 * 5. Если настроек нет — сразу запускается режим AP.
 * 6. Переключение между режимами — через веб-интерфейс (форма настройки Wi-Fi).
 * 7. При переключении — перезагрузка ESP32.
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>   // ДЛЯ УПРАВЛЕНИЯ МОЩНОСТЬЮ

#include "wifi_mqtt.h"
#include "globals.h"
#include "sensors.h"
#include "mode2_timer.h"
#include "mode2_logic.h"
#include "eeprom_settings.h"

// ============================================================================
// КОНСТАНТЫ
// ============================================================================
#define AP_SSID "TermoESP32"
#define AP_PASSWORD ""  // Пустой пароль — открытая сеть
#define AP_CHANNEL 6    // Фиксированный канал для стабильности
#define AP_IP 192, 168, 4, 1
#define AP_GATEWAY 192, 168, 4, 1
#define AP_SUBNET 255, 255, 255, 0
#define STA_TIMEOUT 15000  // 15 секунд на подключение к роутеру
#define AP_TX_POWER 80     // 80 = 20 дБм (максимальная мощность)

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
AsyncWebServer server(80);
bool wifiClientConnected = false;
bool wifiModeAP = true;  // true = AP, false = STA

// Внешние переменные
extern uint32_t timeStartMs;
extern bool timeIsCounting;

// ============================================================================
// ФОРМИРОВАНИЕ JSON С ДАННЫМИ ТЕМПЕРАТУР (6 ДАТЧИКОВ)
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
  float temps[6];
  temps[0] = sensors[0].found ? sensors[0].temp : 0.0f;
  temps[1] = sensors[1].found ? sensors[1].temp : 0.0f;
  temps[2] = sensors[2].found ? sensors[2].temp : 0.0f;
  temps[3] = sensors[3].found ? sensors[3].temp : 0.0f;
  temps[4] = sensors[4].found ? sensors[4].temp : 0.0f;
  temps[5] = sensors[5].found ? sensors[5].temp : 0.0f;

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
// ОБРАБОТЧИК WEBSOCKET
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
      {
        String msg = String((char*)payload);
        if (msg.startsWith("WIFI_SET:")) {
          String data = msg.substring(9);
          int sep = data.indexOf(':');
          if (sep > 0) {
            String ssid = data.substring(0, sep);
            String pass = data.substring(sep + 1);
            settings_save_wifi(ssid.c_str(), pass.c_str());
            webSocket.broadcastTXT("{\"status\":\"saved, rebooting...\"}");
            delay(1000);
            ESP.restart();
          }
        }
        else if (msg == "WIFI_AP") {
          settings_clear_wifi();
          webSocket.broadcastTXT("{\"status\":\"switching to AP...\"}");
          delay(1000);
          ESP.restart();
        }
        else if (msg == "WIFI_STATUS") {
          String status = "{";
          status += "\"mode\":\"" + String(wifiModeAP ? "AP" : "STA") + "\",";
          status += "\"ssid\":\"" + (wifiModeAP ? AP_SSID : WiFi.SSID()) + "\",";
          status += "\"ip\":\"" + (wifiModeAP ? "192.168.4.1" : WiFi.localIP().toString()) + "\"";
          status += "}";
          webSocket.broadcastTXT(status);
        }
      }
      break;
    default:
      break;
  }
}

// ============================================================================
// ЗАПУСК РЕЖИМА AP (С УВЕЛИЧЕННОЙ МОЩНОСТЬЮ)
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

  // Небольшая задержка и установка максимальной мощности
  delay(200);
  esp_wifi_set_max_tx_power(AP_TX_POWER);  // 80 = 20 дБм
  
  wifiModeAP = true;
  Serial.printf("[WiFi] AP запущена. SSID: %s, IP: 192.168.4.1, канал: %d, мощность: 20 дБм\n", 
                AP_SSID, AP_CHANNEL);
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ К РОУТЕРУ (РЕЖИМ STA)
// ============================================================================
bool connectSTA() {
  Serial.println("[WiFi] Попытка подключения к роутеру...");
  WiFi.mode(WIFI_STA);
  
  String ssid = settings_get_ssid();
  String pass = settings_get_password();
  
  if (ssid.length() == 0) {
    Serial.println("[WiFi] Нет сохранённых настроек роутера.");
    return false;
  }
  
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < STA_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiModeAP = false;
    Serial.printf("[WiFi] Подключено к роутеру. IP: %s\n", WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("[WiFi] Не удалось подключиться к роутеру.");
    return false;
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
  // 1. ПРОВЕРКА НАСТРОЕК И ПОДКЛЮЧЕНИЕ К РОУТЕРУ (ЕСЛИ ЕСТЬ)
  // ========================================================================
  if (settings_has_wifi()) {
    bool connected = connectSTA();
    if (connected) {
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
  } else {
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

  while (1) {
    webSocket.loop();

    uint32_t now = millis();
    if (now - lastSend >= 1000) {
      if (wifiClientConnected) {
        sendTemperaturesToClients();
      }
      lastSend = now;
    }

    if (!wifiModeAP) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Связь с роутером потеряна! Переключение в AP...");
        settings_clear_wifi();
        delay(2000);
        ESP.restart();
      }
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
  }
}