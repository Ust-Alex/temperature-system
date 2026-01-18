#include "wifi_ap_module.h"

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ (только для этого файла)
// ============================================================================
static bool wifiInitialized = false; // Флаг успешной инициализации

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ WI-FI МОДУЛЯ
// ============================================================================

/**
 * Упрощённая инициализация Wi-Fi (только точка доступа)
 */
void wifi_ap_setup(void) {
    Serial.println("\n" + String(50, '='));
    Serial.println("[WiFi-SETUP] 🛠️  НАЧАЛО ИНИЦИАЛИЗАЦИИ WI-FI");
    Serial.println(String(50, '='));
    
    // ШАГ 1: Проверка памяти
    Serial.printf("[WiFi-SETUP] Свободная память: %d байт\n", ESP.getFreeHeap());
    delay(500);
    
    // ШАГ 2: Настройка режима Wi-Fi
    Serial.println("[WiFi-SETUP] Настройка режима Wi-Fi...");
    WiFi.mode(WIFI_AP);
    delay(500);
    
    // ШАГ 3: Запуск точки доступа
    Serial.println("[WiFi-SETUP] Запуск точки доступа...");
    bool apStarted = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
    
    if (!apStarted) {
        Serial.println("❌ ОШИБКА: Не удалось запустить точку доступа!");
        return;
    }
    
    delay(1000); // Даём время точке доступа запуститься
    
    // ШАГ 4: Вывод информации
    IPAddress ip = WiFi.softAPIP();
    Serial.println("\n" + String(40, '-'));
    Serial.println("✅ ТОЧКА ДОСТУПА УСПЕШНО ЗАПУЩЕНА");
    Serial.println(String(40, '-'));
    Serial.printf("    SSID:     %s\n", WIFI_AP_SSID);
    Serial.printf("    IP-адрес: %s\n", ip.toString().c_str());
    Serial.printf("    Канал:    %d\n", WIFI_AP_CHANNEL);
    Serial.printf("    Память:   %d байт\n", ESP.getFreeHeap());
    Serial.println(String(40, '-'));
    
    // ШАГ 5: Установка флага
    wifiInitialized = true;
    Serial.println("[WiFi-SETUP] ✅ ИНИЦИАЛИЗАЦИЯ ЗАВЕРШЕНА");
    Serial.println(String(50, '=') + "\n");
}

/**
 * Основной цикл Wi-Fi (упрощённый)
 */
void wifi_ap_loop(void) {
    if (!wifiInitialized) {
        delay(1000);
        return;
    }
    
    // Простая диагностика (раз в 10 секунд)
    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    
    if (now - lastPrint > 10000) {
        Serial.printf("[WiFi-LOOP] Клиентов: %d, Память: %d байт\n",
                     WiFi.softAPgetStationNum(), ESP.getFreeHeap());
        lastPrint = now;
    }
    
    delay(100); // Короткая пауза
}

/**
 * Заглушка для отправки данных (будет реализована позже)
 */
void wifi_ap_send_system_data(const SystemData_t* data) {
    if (!wifiInitialized || !data) return;
    
    static uint32_t lastCall = 0;
    uint32_t now = millis();
    
    if (now - lastCall > 5000) {
        Serial.printf("[WiFi] Данные получены (режим: %d, темп: %.2f)\n",
                     data->mode, data->temps[3]);
        lastCall = now;
    }
}