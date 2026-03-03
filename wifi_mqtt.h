#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <FS.h>

// Явно указываем пространство имён fs
using namespace fs;

// ============================================================================
// ВАЖНО! ПРАВИЛЬНЫЙ ПОРЯДОК ПОДКЛЮЧЕНИЯ БИБЛИОТЕК
// ============================================================================

// 1. Сначала WiFiManager (он тянет стандартный WebServer)
#include <DNSServer.h>
#include <WiFiManager.h>

// 2. Определяем WEBSERVER_H, чтобы ESPAsyncWebServer не создавал свои HTTP-константы
#define WEBSERVER_H

// 3. Теперь подключаем ESPAsyncWebServer — он видит WEBSERVER_H и не конфликтует
#include <ESPAsyncWebServer.h>

// ============================================================================
// КОНСТАНТЫ WiFi
// ============================================================================
#define WEBSOCKET_PORT 8080
#define CONFIG_BUTTON_PIN 0   // GPIO0 (кнопка BOOT)

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern WebSocketsServer webSocket;
extern AsyncWebServer server;
extern bool wifiClientConnected;

// ============================================================================
// ФУНКЦИИ
// ============================================================================
void initWebServer();
void initWebSocket();
void sendTemperaturesToClients();
void broadcastJSON(const char* json);
void taskWiFi(void* pvParameters);

#endif