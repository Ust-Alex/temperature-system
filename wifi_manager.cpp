/**
 * ============================================================================
 * @file wifi_manager.cpp
 * @brief РЕАЛИЗАЦИЯ УПРАВЛЕНИЯ ПОДКЛЮЧЕНИЕМ
 * @version 6.2
 * ============================================================================
 */

#include "wifi_manager.h"
#include "wifi_config.h"
#include "wifi_utils.h"
#include <WiFi.h>
#include <esp_wifi.h>

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (объявлены в других файлах)
// ============================================================================
extern bool wifiModeAP;
extern String lastConnectedSSID;
bool wifiModeAP = true;           // true = AP, false = STA
String lastConnectedSSID = "";    // Последняя успешная сеть


// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: ПОДКЛЮЧЕНИЕ К ОДНОЙ СЕТИ С РЕТРАЯМИ
// ============================================================================
/**
 * @brief Подключается к одной сети с ретраями и статическим IP
 * @param ssid      Имя сети
 * @param password  Пароль
 * @param localIP   Статический IP для ESP32
 * @param gateway   Шлюз
 * @param subnet    Маска подсети
 * @param isLastAttempt Флаг: последняя ли это сеть в списке
 * @return true – подключено, false – не удалось
 */
static bool connectToNetwork(const char* ssid, const char* password, 
                             IPAddress localIP, IPAddress gateway, IPAddress subnet, 
                             bool isLastAttempt) {
    Serial.printf("[WiFi] Сеть %s: попытка 1/%d...\n", ssid, STA_RETRY_ATTEMPTS);
    
    for (int attempt = 1; attempt <= STA_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 1) {
            Serial.printf("[WiFi] Сеть %s: попытка %d/%d...\n", ssid, attempt, STA_RETRY_ATTEMPTS);
        }
        
        // ---- СБРОС Wi-Fi ПЕРЕД КАЖДОЙ ПОПЫТКОЙ ----
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_STA);
        
        // ---- УСТАНОВКА СТАТИЧЕСКОГО IP ----
        if (!WiFi.config(localIP, gateway, subnet)) {
            Serial.println("[WiFi] ⚠️ Не удалось настроить статический IP");
        } else {
            Serial.printf("[WiFi] Статический IP установлен: %d.%d.%d.%d\n", 
                          localIP[0], localIP[1], localIP[2], localIP[3]);
        }
        
        // ---- ЗАПУСК ПОДКЛЮЧЕНИЯ ----
        WiFi.begin(ssid, password);
        
        // ---- ОЖИДАНИЕ С ТАЙМАУТОМ ----
        uint32_t startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < STA_ATTEMPT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        
        // ---- ПРОВЕРКА РЕЗУЛЬТАТА ----
        if (WiFi.status() == WL_CONNECTED) {
            lastConnectedSSID = String(ssid);
            int8_t realPower = getRealTxPower();
            if (realPower >= 0) {
                Serial.printf("[WiFi] ✅ Подключено к %s. IP: %s, мощность: %d дБм\n", 
                              ssid, WiFi.localIP().toString().c_str(), realPower);
            } else {
                Serial.printf("[WiFi] ✅ Подключено к %s. IP: %s, мощность: ОШИБКА\n", 
                              ssid, WiFi.localIP().toString().c_str());
            }
            return true;
        }
        
        // ---- ПАУЗА МЕЖДУ ПОПЫТКАМИ (кроме последней) ----
        if (!(attempt == STA_RETRY_ATTEMPTS && isLastAttempt)) {
            delay(STA_RETRY_DELAY_MS);
        }
    }
    
    Serial.printf("[WiFi] ❌ Не удалось подключиться к %s\n", ssid);
    return false;
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ К РОУТЕРУ (ПЕРЕБОР ДВУХ СЕТЕЙ)
// ============================================================================
bool connectSTA() {
    Serial.println("[WiFi] === НАЧАЛО ПОДКЛЮЧЕНИЯ К РОУТЕРУ ===");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    
    // Установка максимальной мощности (один раз)
    esp_wifi_set_max_tx_power(AP_TX_POWER);
    
    // ---- ПЕРВАЯ СЕТЬ ----
    IPAddress ip1(STA1_IP);
    IPAddress gw1(STA1_GATEWAY);
    IPAddress sn1(STA1_SUBNET);
    bool connected = connectToNetwork(MY_SSID_1, MY_PASS_1, ip1, gw1, sn1, false);
    
    // ---- ВТОРАЯ СЕТЬ (если первая не подошла) ----
    if (!connected) {
        Serial.printf("[WiFi] Пробую вторую сеть: %s...\n", MY_SSID_2);
        IPAddress ip2(STA2_IP);
        IPAddress gw2(STA2_GATEWAY);
        IPAddress sn2(STA2_SUBNET);
        connected = connectToNetwork(MY_SSID_2, MY_PASS_2, ip2, gw2, sn2, true);
    }
    
    // ---- РЕЗУЛЬТАТ ----
    if (connected) {
        wifiModeAP = false;
        Serial.printf("[WiFi] === УСПЕШНО ПОДКЛЮЧЕНО к %s ===\n", lastConnectedSSID.c_str());
        return true;
    } else {
        wifiModeAP = true;
        Serial.println("[WiFi] === НЕ УДАЛОСЬ ПОДКЛЮЧИТЬСЯ НИ К ОДНОЙ СЕТИ ===");
        return false;
    }
}

// ============================================================================
// ПЕРЕПОДКЛЮЧЕНИЕ ПРИ ПОТЕРЕ СВЯЗИ
// ============================================================================
bool reconnectSTA() {
    Serial.println("[WiFi] === ПОПЫТКА ВОССТАНОВЛЕНИЯ СВЯЗИ ===");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_max_tx_power(AP_TX_POWER);
    
    // ---- 1. СНАЧАЛА ПРОБУЕМ ПОСЛЕДНЮЮ УСПЕШНУЮ СЕТЬ ----
    if (lastConnectedSSID.length() > 0) {
        Serial.printf("[WiFi] Пробую последнюю успешную сеть: %s...\n", lastConnectedSSID.c_str());
        
        IPAddress ip, gw, sn;
        const char* pass = "";
        
        if (lastConnectedSSID == MY_SSID_1) {
            ip = IPAddress(STA1_IP);
            gw = IPAddress(STA1_GATEWAY);
            sn = IPAddress(STA1_SUBNET);
            pass = MY_PASS_1;
        } else if (lastConnectedSSID == MY_SSID_2) {
            ip = IPAddress(STA2_IP);
            gw = IPAddress(STA2_GATEWAY);
            sn = IPAddress(STA2_SUBNET);
            pass = MY_PASS_2;
        } else {
            Serial.printf("[WiFi] ⚠️ Неизвестная сеть: %s\n", lastConnectedSSID.c_str());
            return false;
        }
        
        bool connected = connectToNetwork(lastConnectedSSID.c_str(), pass, ip, gw, sn, false);
        if (connected) {
            wifiModeAP = false;
            Serial.printf("[WiFi] === СВЯЗЬ ВОССТАНОВЛЕНА с %s ===\n", lastConnectedSSID.c_str());
            return true;
        }
    }
    
    // ---- 2. ЕСЛИ НЕ ПОЛУЧИЛОСЬ — ПЕРЕБИРАЕМ ВСЕ СЕТИ ПО ПОРЯДКУ ----
    Serial.println("[WiFi] Перебираю все сети по порядку...");
    
    IPAddress ip1(STA1_IP);
    IPAddress gw1(STA1_GATEWAY);
    IPAddress sn1(STA1_SUBNET);
    bool connected = connectToNetwork(MY_SSID_1, MY_PASS_1, ip1, gw1, sn1, false);
    
    if (!connected) {
        IPAddress ip2(STA2_IP);
        IPAddress gw2(STA2_GATEWAY);
        IPAddress sn2(STA2_SUBNET);
        connected = connectToNetwork(MY_SSID_2, MY_PASS_2, ip2, gw2, sn2, true);
    }
    
    if (connected) {
        wifiModeAP = false;
        Serial.printf("[WiFi] === СВЯЗЬ ВОССТАНОВЛЕНА с %s ===\n", lastConnectedSSID.c_str());
        return true;
    } else {
        wifiModeAP = true;
        Serial.println("[WiFi] === ВОССТАНОВИТЬ СВЯЗЬ НЕ УДАЛОСЬ ===");
        return false;
    }
}

// ============================================================================
// ЗАПУСК РЕЖИМА AP (ТОЧКА ДОСТУПА)
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
    
    delay(200);
    esp_wifi_set_max_tx_power(AP_TX_POWER);
    
    wifiModeAP = true;
    lastConnectedSSID = "";  // Сбрасываем запомненную сеть
    
    // Получаем реальную мощность
    int8_t realPower = getRealTxPower();
    if (realPower >= 0) {
        Serial.printf("[WiFi] AP запущена. SSID: %s, IP: 192.168.4.1, канал: %d, мощность: %d дБм\n", 
                      AP_SSID, AP_CHANNEL, realPower);
    } else {
        Serial.printf("[WiFi] AP запущена. SSID: %s, IP: 192.168.4.1, канал: %d, мощность: ОШИБКА\n", 
                      AP_SSID, AP_CHANNEL);
    }
}