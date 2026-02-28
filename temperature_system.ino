/**
 * ============================================================================
 * ФАЙЛ: temperature_system.ino
 * ГЛАВНЫЙ ФАЙЛ ПРОЕКТА - ТОЧКА ВХОДА
 * 
 * ВЕРСИЯ: 5.4 (WiFi точка доступа без пароля, исправлен порядок инициализации)
 * ============================================================================
 */

#include <WiFi.h>
#include "eeprom_settings.h"
#include "system_config.h"
#include "rtos_tasks.h"
#include "calibration_simple.h"
#include "mp3_player.h"
#include "mode2_timer.h"
#include "sensors.h"
#include "menu_engine.h"

void setup() {
  // ==========================================================================
  // 1. Serial порт — ПЕРВЫМ, чтобы видеть все сообщения
  // ==========================================================================
  Serial.begin(115200);
  delay(2000);

  // ==========================================================================
  // 2. WiFi точка доступа (без пароля — работает стабильно)
  // ==========================================================================
  Serial.println("\n[WiFi] Запуск точки доступа...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  
  if (WiFi.softAP("TermoESP32")) {  // Без пароля!
    Serial.println("[WiFi] ✅ Точка доступа создана: TermoESP32");
    Serial.printf("[WiFi] IP адрес: %s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[WiFi] ❌ ОШИБКА: Не удалось создать точку доступа!");
    Serial.printf("  Режим WiFi: %d\n", WiFi.getMode());
    Serial.printf("  Статус: %d\n", WiFi.status());
    Serial.printf("  MAC адрес: %s\n", WiFi.softAPmacAddress().c_str());
  }
  delay(1000);

  // ==========================================================================
  // 3. Настройки EEPROM
  // ==========================================================================
  settings_init();

  // ==========================================================================
  // 4. Заголовок
  // ==========================================================================
  Serial.println("\n" + String(70, '='));
  Serial.println("🚀 СИСТЕМА КОНТРОЛЯ ТЕМПЕРАТУРЫ v5.4");
  Serial.println(String(70, '='));

  // ==========================================================================
  // 5. Аппаратная диагностика
  // ==========================================================================
  Serial.println("\n🔍 ПРЕДВАРИТЕЛЬНАЯ ДИАГНОСТИКА:");
  Serial.printf("  Датчик гильзы: GPIO%d\n", ONE_WIRE_BUS_A);
  Serial.printf("  Датчики стенок: GPIO%d\n", ONE_WIRE_BUS_B);

  pinMode(ONE_WIRE_BUS_A, INPUT_PULLUP);
  pinMode(ONE_WIRE_BUS_B, INPUT_PULLUP);
  delay(50);

  if (digitalRead(ONE_WIRE_BUS_A) == LOW || digitalRead(ONE_WIRE_BUS_B) == LOW) {
    Serial.println("\n❌ КРИТИЧЕСКАЯ ОШИБКА АППАРАТУРЫ!");
    while (true) delay(1000);
  }

  // ==========================================================================
  // 6. Инициализация MP3
  // ==========================================================================
  Serial.println("\n[MP3] Инициализация...");
  if (initMP3Player()) {
    Serial.println("✅ MP3-проигрыватель готов");
  } else {
    Serial.println("⚠️ MP3 не обнаружен");
  }

  // ==========================================================================
  // 7. Основная инициализация аппаратуры
  // ==========================================================================
  Serial.println("\n🔄 Запуск основной инициализации...");
  initHardware();
  calibration_init();

  // ==========================================================================
  // 8. Создание задач FreeRTOS
  // ==========================================================================
  Serial.println("\n[INIT] Создание задач...");
  create_rtos_tasks();

  menu_init();

  // ==========================================================================
  // 9. Запуск задачи MP3 и стартовый трек
  // ==========================================================================
  if (mp3PlayerReady && mp3CommandQueue != NULL) {
    xTaskCreate(taskMP3, "MP3 Player", 4096, NULL, 1, NULL);
    Serial.println("✅ Задача MP3 создана");

    vTaskDelay(pdMS_TO_TICKS(500));

    Mp3Command_t startSound = { MP3_CMD_PLAY_TRACK, 1 };
    sendMP3Command(startSound);
    Serial.println("🎵 Стартовый трек #1 отправлен");
  }

  // ==========================================================================
  // 10. Финальное сообщение
  // ==========================================================================
  Serial.println("\n" + String(70, '='));
  Serial.println("✅ СИСТЕМА ЗАПУЩЕНА");
  Serial.println(String(70, '=') + "\n");
}

void loop() {
  static uint32_t lastSystemCheck = 0;
  uint32_t currentMillis = millis();

  // Периодическая проверка (раз в 5 минут)
  if (currentMillis - lastSystemCheck > 300000) {
    Serial.println("\n[SYSTEM CHECK]");
    Serial.printf("Режим: %d, Гильза: %s, Ошибка: %s\n",
                  sysData.mode,
                  sensors[3].found ? "✅" : "❌",
                  criticalError ? "❌" : "✅");
    lastSystemCheck = currentMillis;
  }

  vTaskDelay(pdMS_TO_TICKS(1000));
}