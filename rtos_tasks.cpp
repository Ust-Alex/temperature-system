/**
 * ============================================================================
 * @file rtos_tasks.cpp
 * @brief ЗАДАЧИ FREERTOS (исправленная версия, калибровка через eeprom)
 * @version 4.3
 * 
 * ОСОБЕННОСТИ:
 * - 4 задачи: энкодер, измерения, дисплей, serial
 * - Очередь событий для энкодера
 * - Таймаут возврата в главный экран (30 сек)
 * - Команды калибровки работают через eeprom_settings
 * ============================================================================
 */

#include "rtos_tasks.h"
#include "measurement_task.h"
#include "sensors.h"
#include "encoder_engine.h"
#include "calibration_simple.h"
#include "display_engine.h"
#include "eeprom_settings.h"
#include "wifi_mqtt.h"

// ============================================================================
// КОНСТАНТЫ
// ============================================================================
#define HEARTBEAT_INTERVAL 30000
#define STACK_CHECK_INTERVAL 300000
#define ENCODER_POLL_INTERVAL 10
#define INACTIVITY_TIMEOUT 30000

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static uint8_t selectedModeIndex = 0;
static uint32_t lastUserActivity = 0;

// ============================================================================
// ЗАДАЧА ЭНКОДЕРА
// ============================================================================
void taskEncoder(void* pv) {
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    encoder_tick();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(ENCODER_POLL_INTERVAL));
  }
}

// ============================================================================
// ЗАДАЧА SERIAL ИНТЕРФЕЙСА
// ============================================================================
void taskSerial(void* pv) {
  uint32_t lastHeartbeat = 0;
  uint32_t cmdCount = 0;

  Serial.println("📟 Serial задача запущена");
  Serial.println("🎛️  HELP - список команд");

  while (1) {
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

    if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
      // Serial.printf("[SERIAL] heartbeat, команд: %lu\n", cmdCount);
      lastHeartbeat = now;
    }

    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      cmd.toUpperCase();
      cmdCount++;

      Serial.printf("> %s\n", cmd.c_str());

      // =================================================================
      // СИСТЕМНЫЕ
      // =================================================================
      if (cmd == "HELP" || cmd == "?") {
        Serial.println("\n" + String(60, '='));
        Serial.println("📋 КОМАНДЫ:");
        Serial.println("  HELP        - список");
        Serial.println("  STATUS      - статус системы");
        Serial.println("  FIND        - поиск датчиков");
        Serial.println("  REBOOT      - перезагрузка");
        Serial.println("  MODE1/2     - переключение режима");
        Serial.println("\n📊 КАЛИБРОВКА:");
        Serial.println("  CALIB SHOW");
        Serial.println("  CALIB AUTO");
        Serial.println("  CALIB ON/OFF");
        Serial.println("  CALIB RESET");
        Serial.println("  CALIB REF N");
        Serial.println("  CALIB SET N X");
        Serial.println(String(60, '='));
      }

      else if (cmd == "STATUS") {
        Serial.println("\n" + String(50, '='));
        Serial.println("СТАТУС");
        Serial.println(String(50, '='));
        Serial.printf("Режим: %s\n", sysData.mode ? "MODE2" : "MODE1");
        Serial.printf("Инициализация: %s\n", systemInitialized ? "ДА" : "НЕТ");
        Serial.printf("База гильзы: %.2f\n", guildBaseTemp);

        Serial.println("\n--- ДАТЧИКИ ---");
        for (int i = 0; i < 4; i++) {
          Serial.printf("  [%d] %s: ", i, sensorNames[i]);
          if (!sensors[i].found) {
            Serial.println("❌ не найден");
          } else {
            float t = sysData.temps[i];
            if (t == TEMP_NO_DATA) Serial.print("⚠️ нет данных");
            else if (t == TEMP_SENSOR_LOST) Serial.print("❌ потерян");
            else if (t == TEMP_CRITICAL_LOST) Serial.print("🔥 критично");
            else Serial.printf("%.2f°C, Δ%.2f", t, sysData.deltas[i]);
            Serial.println();
          }
        }

        Serial.println("\n--- ОЧЕРЕДИ ---");
        if (dataQueue) Serial.printf("  Данные: %d мест\n", uxQueueSpacesAvailable(dataQueue));
        if (eventQueue) Serial.printf("  События: %d мест\n", uxQueueSpacesAvailable(eventQueue));
        Serial.println(String(50, '='));
      }

      else if (cmd == "FIND") {
        sensors_scan_all();
        forceDisplayRedraw = true;
      }

      else if (cmd == "REBOOT") {
        Serial.println("🔄 Перезагрузка...");
        delay(1000);
        ESP.restart();
      }

      // =================================================================
      // РЕЖИМЫ
      // =================================================================
      else if (cmd == "MODE1") {
        resetDisplayState(0);
        Serial.println("🔵 MODE1");
      } else if (cmd == "MODE2") {
        resetDisplayState(1);
        Serial.println("🟢 MODE2");
      }

      // =================================================================
      // КАЛИБРОВКА (всё через eeprom)
      // =================================================================
      else if (cmd == "CALIB SHOW") {
        printCalibrationStatus();
      } else if (cmd == "CALIB AUTO") {
        autoCalibrateAllSensors();
      } else if (cmd == "CALIB ON") {
        toggleCalibration(true);
      } else if (cmd == "CALIB OFF") {
        toggleCalibration(false);
      } else if (cmd == "CALIB RESET") {
        for (int i = 0; i < 4; i++) settings_set_offset(i, 0.0f);
        settings_save();
        Serial.println("[CALIB] ✅ Все offset сброшены");
      } else if (cmd.startsWith("CALIB REF ")) {
        int idx = cmd.substring(10).toInt();
        if (idx >= 0 && idx < 4) setReferenceSensor(idx);
        else Serial.println("[CALIB] ❌ Индекс 0-3");
      } else if (cmd.startsWith("CALIB SET ")) {
        int sp1 = 9;
        int sp2 = cmd.indexOf(' ', sp1 + 1);
        if (sp2 > 0) {
          int idx = cmd.substring(sp1, sp2).toInt();
          float off = cmd.substring(sp2 + 1).toFloat();
          if (idx >= 0 && idx < 4) setManualOffset(idx, off);
          else Serial.println("[CALIB] ❌ Индекс 0-3");
        } else {
          Serial.println("[CALIB] ❌ Формат: CALIB SET [0-3] [offset]");
        }
      }

      else {
        Serial.println("❌ Неизвестно. HELP - список.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ОБЪЕКТОВ FREERTOS
// ============================================================================
void initFreeRTOSObjects() {
  Serial.println("[RTOS] Создание объектов...");

  dataQueue = xQueueCreate(20, sizeof(SystemData_t));
  Serial.println(dataQueue ? "   ✅ Очередь данных" : "   ❌ Ошибка");

  dataMutex = xSemaphoreCreateMutex();
  Serial.println(dataMutex ? "   ✅ Мьютекс" : "   ❌ Ошибка");

  eventQueue = xQueueCreate(10, sizeof(uint8_t));
  Serial.println(eventQueue ? "   ✅ Очередь событий" : "   ❌ Ошибка");
}

// ============================================================================
// СОЗДАНИЕ ВСЕХ ЗАДАЧ
// ============================================================================
void create_rtos_tasks() {
  Serial.println("\n[RTOS] Создание задач...");

  xTaskCreate(taskEncoder, "Encoder", 2048, NULL, 4, NULL);
  xTaskCreate(taskMeasure, "Measure", 4096, NULL, 3, NULL);
  xTaskCreate(taskDisplay, "Display", 4096, NULL, 2, NULL);
  xTaskCreate(taskSerial, "Serial", 3072, NULL, 1, NULL);
  xTaskCreate(taskWiFi, "WiFi", 4096, NULL, 1, NULL);
  Serial.println("   ✅ Задача WiFi создана");
  Serial.println("[RTOS] Все задачи созданы");
}