#include "serial_interface.h"

void serial_process_command(String command) {
  command.trim();
  
  if (command.length() == 0) return;
  
  command.toUpperCase();
  
  Serial.print("[CMD] Получена команда: '");
  Serial.print(command);
  Serial.println("'");
  
  if (command == "MODE1") {
    resetDisplayState(0);
    Serial.println("✅ Установлен режим: СТАБИЛИЗАЦИЯ");
    Serial.println("   Экран будет полностью перерисован синим фоном");
    
  } else if (command == "MODE2") {
    resetDisplayState(1);
    Serial.println("✅ Установлен режим: РАБОЧИЙ");
    Serial.printf("   Базовая температура гильзы: %.2f°C\n", guildBaseTemp);
    Serial.println("   Цвет фона будет меняться относительно этой температуры");
    
  } else if (command == "RESET") {
    Serial.println("\n🔄 ПОЛНЫЙ СБРОС СИСТЕМЫ...");
    
    baseSaved = false;
    guildBaseTemp = 0.0f;
    guildColorState = 0;
    timeRefTemp = 0.0f;
    timeIsCounting = false;
    forceDisplayRedraw = true;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sysData.mode = 0;
      sysData.needsRedraw = true;
      xSemaphoreGive(dataMutex);
    }
    
    Serial.println("✅ Система полностью сброшена");
    Serial.println("   Режим установлен: СТАБИЛИЗАЦИЯ (MODE1)");
    
  } else if (command == "FIND") {
    Serial.println("\n🔍 ПРИНУДИТЕЛЬНЫЙ ПОИСК ДАТЧИКОВ...");
    findSensors();
    forceDisplayRedraw = true;
    Serial.println("✅ Поиск завершен, дисплей будет обновлен");
    
  } else if (command == "STATUS") {
    serial_print_status();
    
  } else if (command == "HELP" || command == "?") {
    serial_print_help();
    
  } else {
    Serial.println("❌ Неизвестная команда!");
    Serial.println("   Доступные команды: MODE1, MODE2, RESET, FIND, STATUS, HELP");
    Serial.println("   Введите HELP для подробной справки");
  }
}

void serial_handle_input() {
  static String inputBuffer = "";
  static uint32_t lastCommandTime = 0;
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      
      if (inputBuffer.length() > 0) {
        uint32_t now = millis();
        if (now - lastCommandTime < 200) {
          Serial.println("⏳ Слишком быстро! Подождите немного...");
          inputBuffer = "";
          continue;
        }
        lastCommandTime = now;
        
        serial_process_command(inputBuffer);
        inputBuffer = "";
      }
    } else if (c >= 32 && c <= 126) {
      if (inputBuffer.length() < 64) {
        inputBuffer += c;
      } else {
        Serial.println("⚠️  Слишком длинная команда! Максимум 64 символа.");
        inputBuffer = "";
      }
    }
  }
}

void serial_print_status() {
  Serial.println("\n" + String(50, '='));
  Serial.println("         СТАТУС СИСТЕМЫ");
  Serial.println(String(50, '='));
  
  SystemData_t currentData;
  safeReadSystemData(&currentData);
  
  Serial.printf("Режим работы: %s\n",
                currentData.mode == 0 ? "СТАБИЛИЗАЦИЯ (MODE1)" : "РАБОЧИЙ (MODE2)");
  Serial.printf("Система инициализирована: %s\n",
                systemInitialized ? "ДА" : "НЕТ");
  Serial.printf("Критическая ошибка: %s\n",
                criticalError ? "ДА (гильза!)" : "НЕТ");
  Serial.printf("Базовая темп. гильзы: %.2f°C\n",
                guildBaseTemp);
  Serial.printf("Текущее цвет. состояние: ");
  switch (guildColorState) {
    case 0: Serial.println("ЗЕЛЁНЫЙ"); break;
    case 1: Serial.println("ЖЁЛТЫЙ"); break;
    case 2: Serial.println("КРАСНЫЙ"); break;
    default: Serial.println("НЕИЗВЕСТНО"); break;
  }
  Serial.printf("Флаг перерисовки: %s\n",
                forceDisplayRedraw ? "ДА" : "НЕТ");
  
  Serial.println("\n--- СОСТОЯНИЕ ДАТЧИКОВ ---");
  for (int i = 0; i < 4; i++) {
    Serial.printf("  [%d] %s: ", i, sensorNames[i]);
    if (sensors[i].found) {
      Serial.printf("✅ Найден, ");
      if (isValidTemperature(currentData.temps[i])) {
        Serial.printf("%.2f°C", currentData.temps[i]);
        // ДЕЛЬТА УДАЛЕНА - больше не выводится
      } else {
        Serial.printf("ОШИБКА ДАННЫХ");
      }
    } else {
      Serial.printf("❌ Не найден");
    }
    Serial.println();
  }
  
  Serial.println("\n--- ЗАДАЧИ FREERTOS ---");
  Serial.printf("  Очередь данных: %s\n",
                dataQueue != NULL ? "Создана" : "ОШИБКА");
  Serial.printf("  Свободное место в очереди: %d\n",
                uxQueueSpacesAvailable(dataQueue));
  Serial.printf("  Мьютекс данных: %s\n",
                dataMutex != NULL ? "Создан" : "ОШИБКА");
  
  Serial.println(String(50, '=') + "\n");
}

void serial_print_help() {
  Serial.println("\n" + String(50, '='));
  Serial.println("         КОМАНДЫ СИСТЕМЫ");
  Serial.println(String(50, '='));
  Serial.println("MODE1   - Режим стабилизации (синий фон)");
  Serial.println("          Полный сброс дисплея");
  Serial.println("");
  Serial.println("MODE2   - Рабочий режим (цвет зависит от гильзы)");
  Serial.println("          Сохраняет текущую температуру гильзы");
  Serial.println("          Полный сброс дисплея");
  Serial.println("");
  Serial.println("RESET   - Полный сброс системы");
  Serial.println("          Сбрасывает режим, базовую температуру");
  Serial.println("          и все настройки");
  Serial.println("");
  Serial.println("FIND    - Принудительный поиск датчиков");
  Serial.println("          Перепривязка адресов, обновление дисплея");
  Serial.println("");
  Serial.println("STATUS  - Подробный статус системы");
  Serial.println("          Датчики, режим, ошибки, задачи FreeRTOS");
  Serial.println("");
  Serial.println("HELP    - Эта справка");
  Serial.println(String(50, '=') + "\n");
}