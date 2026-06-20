#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <FS.h>

// ============================================================================
// ПОДКЛЮЧЕНИЕ БИБЛИОТЕК (ГЛОБАЛЬНАЯ УСТАНОВКА)
// ============================================================================

// ESPAsyncWebServer — глобально установленная библиотека
#include <ESPAsyncWebServer.h>

// ============================================================================
// КОНСТАНТЫ
// ============================================================================
#define WEBSOCKET_PORT 8080
#define AP_SSID "TermoESP32"       // Имя точки доступа по умолчанию
#define AP_IP_ADDR 192,168,200,1   // IP-адрес точки доступа

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern WebSocketsServer webSocket;
extern AsyncWebServer server;
extern bool wifiClientConnected;
extern bool wifiModeAP;  // true = AP, false = STA

// ============================================================================
// ФУНКЦИИ
// ============================================================================
void initWebServer();
void initWebSocket();
void sendTemperaturesToClients();
void broadcastJSON(const char* json);
void taskWiFi(void* pvParameters);

// Функции управления режимом Wi-Fi
void startAP();
bool connectSTA();

#endif // WIFI_MQTT_H