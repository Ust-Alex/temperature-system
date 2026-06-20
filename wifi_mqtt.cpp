/**
 * ============================================================================
 * @file wifi_mqtt.cpp
 * @brief Веб-сервер + WebSocket + WiFiManager + mDNS
 * @version 4.0 (6 ДАТЧИКОВ, JSON С МАССИВОМ temps[6])
 * 
 * ОСОБЕННОСТИ:
 * - WiFiManager для подключения к роутеру (режим STA)
 * - mDNS для доступа по имени http://esp32ust.local
 * - Веб-сервер на порту 80 отдаёт index.html из LittleFS
 * - WebSocket на порту 8080 передаёт данные в реальном времени
 * - Кнопка BOOT (GPIO0) для сброса настроек WiFi
 * 
 * ИЗМЕНЕНИЯ ВЕРСИИ 4.0:
 * - JSON теперь содержит массив temps[6] вместо отдельных полей
 * - Порядок датчиков: 0-ВЫХОД, 1-СТЕНКА100, 2-СТЕНКА75, 3-СТЕНКА50, 4-ГИЛЬЗА, 5-КУБ
 * - При отсутствии гильзы (индекс 4) в массиве передаётся 0.00
 * - Дельта удалена
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
// #include "ESPAsyncWebServer.h" // V-3.10.0
#include "src/ESP_Async_WebServer/src/ESPAsyncWebServer.h" // V-3.10.0

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
// ФОРМИРОВАНИЕ JSON С ДАННЫМИ ТЕМПЕРАТУР (6 ДАТЧИКОВ)
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
  // ========================================================================
  // 1. ЧТЕНИЕ ТЕМПЕРАТУР В НОВОМ ПОРЯДКЕ (0-5)
  // ========================================================================
  float temps[6];
  
  // Датчик 0: ВЫХОД (шина C, GPIO21)
  temps[0] = sensors[0].found ? sensors[0].temp : 0.0f;
  
  // Датчик 1: СТЕНКА 100см (шина B, GPIO16)
  temps[1] = sensors[1].found ? sensors[1].temp : 0.0f;
  
  // Датчик 2: СТЕНКА 75см (шина B, GPIO16)
  temps[2] = sensors[2].found ? sensors[2].temp : 0.0f;
  
  // Датчик 3: СТЕНКА 50см (шина B, GPIO16)
  temps[3] = sensors[3].found ? sensors[3].temp : 0.0f;
  
  // Датчик 4: ГИЛЬЗА (шина A, GPIO4) — если не найден, отправляем 0.00
  temps[4] = sensors[4].found ? sensors[4].temp : 0.0f;
  
  // Датчик 5: КУБ (шина D, GPIO22)
  temps[5] = sensors[5].found ? sensors[5].temp : 0.0f;

  // ========================================================================
  // 2. РЕЖИМ, ЦВЕТ, ВРЕМЯ, БАЗОВАЯ ТЕМПЕРАТУРА
  // ========================================================================
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

  // ========================================================================
  // 3. ФОРМИРОВАНИЕ JSON С МАССИВОМ temps[6]
  // ========================================================================
  // Формат: {"temps":[25.0,24.5,24.3,24.1,23.8,23.5],"mode":1,"color":0,"time":"12:30","baseTemp":23.0}
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

// ============================================================================
// ЗАПУСК WI-FI КОНФИГУРАТОРА (ДЛЯ МЕНЮ SETUP)
// ============================================================================
void startWiFiConfig() {
  // Очищаем экран и выводим сообщение
  tft.fillScreen(COLOR_BLACK);
  tft.setTextFont(FONT_DELTA);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  tft.setCursor(57, 50);
  tft.print("Connect to");
  tft.setCursor(47, 80);
  tft.print("TermoESP32");
  tft.setCursor(65, 120);
  tft.print("then open");
  tft.setCursor(55, 150);
  tft.print("192.168.4.1");
  delay(2000);  // показываем сообщение 2 секунды

  // Создаём WiFiManager
  WiFiManager wm;

  // Сбрасываем сохранённые настройки
  wm.resetSettings();

  // Запускаем портал настройки (точка доступа TermoESP32)
  wm.autoConnect("TermoESP32");

  // После завершения (успех или отмена) ждём 2 секунды и перезагружаемся
  delay(2000);
  ESP.restart();
}