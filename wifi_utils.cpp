/**
 * ============================================================================
 * @file wifi_utils.cpp
 * @brief РЕАЛИЗАЦИЯ ВСПОМОГАТЕЛЬНЫХ ФУНКЦИЙ (С TX И RSSI)
 * @version 6.3
 * ============================================================================
 */

#include "wifi_utils.h"
#include "wifi_config.h"
#include "globals.h"
#include "sensors.h"
#include "mode2_timer.h"
#include "mode2_logic.h"
#include <esp_wifi.h>
#include <WebSocketsServer.h>
#include <WiFi.h>  // 👈 ДОБАВЛЕНО для WiFi.RSSI()

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern WebSocketsServer webSocket;
extern bool wifiClientConnected;
extern uint32_t timeStartMs;
extern bool timeIsCounting;

// ============================================================================
// ПОЛУЧЕНИЕ РЕАЛЬНОЙ МОЩНОСТИ
// ============================================================================
int8_t getRealTxPower() {
    int8_t power = 0;
    if (esp_wifi_get_max_tx_power(&power) == ESP_OK) {
        return power / 4;  // Преобразуем из единиц 0.25 дБм в дБм
    }
    return -1;  // Ошибка
}

// ============================================================================
// ФОРМИРОВАНИЕ JSON С ТЕМПЕРАТУРАМИ, TX И RSSI
// ============================================================================
static void buildTemperaturesJSON(char* buffer, size_t bufferSize) {
    // ---- Сбор температур с 6 датчиков ----
    float temps[6];
    for (int i = 0; i < 6; i++) {
        temps[i] = sensors[i].found ? sensors[i].temp : 0.0f;
    }
    
    int mode = sysData.mode;
    int color = guildColorState;
    
    // ---- Формирование строки времени ----
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
    
    // ---- Получение TX и RSSI ----
    int8_t txPower = getRealTxPower();
    int rssi = WiFi.RSSI();
    
    // ---- Сборка JSON с добавленными полями ----
    snprintf(buffer, bufferSize,
             "{"
             "\"temps\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
             "\"mode\":%d,"
             "\"color\":%d,"
             "\"time\":\"%s\","
             "\"baseTemp\":%.2f,"
             "\"tx\":%d,"
             "\"rssi\":%d"
             "}",
             temps[0], temps[1], temps[2], temps[3], temps[4], temps[5],
             mode, color, timeStr, baseTemp,
             txPower,
             rssi
    );
}

// ============================================================================
// ОТПРАВКА ДАННЫХ
// ============================================================================
void broadcastJSON(const char* json) {
    if (wifiClientConnected) {
        webSocket.broadcastTXT(json);
    }
}

void sendTemperaturesToClients() {
    char jsonBuffer[200];  // Увеличил размер буфера для новых полей
    buildTemperaturesJSON(jsonBuffer, sizeof(jsonBuffer));
    broadcastJSON(jsonBuffer);
}

void sendSoundCommand(const char* soundType) {
    if (!wifiClientConnected) return;
    
    char soundMsg[64];
    snprintf(soundMsg, sizeof(soundMsg), "{\"sound\":\"%s\"}", soundType);
    webSocket.broadcastTXT(soundMsg);
    Serial.printf("[WiFi] Отправлен звук: %s\n", soundType);
}