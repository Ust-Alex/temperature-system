#ifndef WIFI_AP_MODULE_H
#define WIFI_AP_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include "system_config.h"

// ============================================================================
// НАСТРОЙКИ WI-FI ТОЧКИ ДОСТУПА
// ============================================================================
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID      "Temperature_System"
#endif

#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD  "termo1234"
#endif

#ifndef WIFI_AP_CHANNEL
#define WIFI_AP_CHANNEL   6
#endif

// ============================================================================
// ПРОТОТИПЫ ФУНКЦИЙ WI-FI МОДУЛЯ (только они!)
// ============================================================================

/**
 * Инициализация Wi-Fi точки доступа
 * Вызывается из задачи taskWiFi
 */
void wifi_ap_setup(void);

/**
 * Основной цикл обработки сетевых событий
 * Вызывается постоянно в задаче taskWiFi
 */
void wifi_ap_loop(void);

/**
 * Отправка данных системы по Wi-Fi
 * @param data Указатель на структуру SystemData_t
 */
void wifi_ap_send_system_data(const SystemData_t* data);

#endif // WIFI_AP_MODULE_H