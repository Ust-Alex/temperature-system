#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

#define WEBSOCKET_PORT 8080
#define AP_SSID "TermoESP32"
#define AP_IP_ADDR 192,168,4,1

extern WebSocketsServer webSocket;
extern AsyncWebServer server;
extern bool wifiClientConnected;
extern bool wifiModeAP;

void initWebServer();
void initWebSocket();
void sendTemperaturesToClients();
void broadcastJSON(const char* json);
void taskWiFi(void* pvParameters);
void startAP();
bool connectSTA();

// ============================================================================
// НОВАЯ ФУНКЦИЯ ДЛЯ ОТПРАВКИ ЗВУКОВЫХ КОМАНД В ВЕБ-ИНТЕРФЕЙС
// ============================================================================
void sendSoundCommand(const char* soundType);

#endif // WIFI_MQTT_H