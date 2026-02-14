#include "sensors.h"
#include "system_config.h"
#include "globals.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// ============================================================================
static float filterBuffer[4][5] = {0};  // Буферы фильтра для каждого датчика
static int filterIndex[4] = {0};         // Текущий индекс в буфере
static float filterSum[4] = {0};         // Сумма для быстрого расчёта среднего

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void sensors_init() {
  Serial.println("[SENSORS] Инициализация модуля датчиков");
  
  // Запускаем шины
  sensorsA.begin();
  sensorsB.begin();
  
  // Устанавливаем разрешение
  sensorsA.setResolution(RESOLUTION);
  sensorsB.setResolution(RESOLUTION);
  
  // Первоначальный поиск
  sensors_scan_all();
}

// ============================================================================
// ПОИСК ДАТЧИКОВ
// ============================================================================
void sensors_scan_all() {
  Serial.println("[SENSORS] Поиск датчиков на шинах...");
  
  // Сбрасываем флаги
  for (int i = 0; i < 4; i++) {
    sensors[i].found = false;
  }
  
  // Поиск на шине A (гильза)
  DeviceAddress addrA;
  if (sensorsA.getAddress(addrA, 0)) {
    sensors[3].found = true;
    memcpy(sensors[3].addr, addrA, 8);
    Serial.printf("  Найден датчик гильзы: ");
    sensors_print_address(3);
  }
  
  // Поиск на шине B (стенки)
  for (int i = 0; i < sensorsB.getDeviceCount(); i++) {
    DeviceAddress addrB;
    if (sensorsB.getAddress(addrB, i)) {
      // Здесь должна быть логика сопоставления адресов с индексами 0,1,2
      // Пока просто записываем в первый свободный
      for (int j = 0; j < 3; j++) {
        if (!sensors[j].found) {
          sensors[j].found = true;
          memcpy(sensors[j].addr, addrB, 8);
          Serial.printf("  Найден датчик стенки %d: ", j);
          sensors_print_address(j);
          break;
        }
      }
    }
  }
  
  Serial.printf("[SENSORS] Найдено датчиков: %d/%d\n", 
                (sensors[0].found + sensors[1].found + sensors[2].found + sensors[3].found), 4);
}

// ============================================================================
// ОПРОС ДАТЧИКОВ
// ============================================================================
void sensors_request_temperatures() {
  sensorsA.requestTemperatures();
  sensorsB.requestTemperatures();
}

void sensors_read_temperatures() {
  for (int i = 0; i < 4; i++) {
    if (sensors[i].found) {
      float rawTemp;
      if (i == 3) {  // Гильза на шине A
        rawTemp = sensorsA.getTempC(sensors[i].addr);
      } else {        // Стенки на шине B
        rawTemp = sensorsB.getTempC(sensors[i].addr);
      }
      
      // Проверка валидности
      if (rawTemp == DEVICE_DISCONNECTED_C || rawTemp < -55 || rawTemp > 125) {
        sensors[i].temp = TEMP_SENSOR_LOST;
      } else {
        sensors[i].temp = rawTemp;
      }
    }
  }
}

void sensors_update_all() {
  sensors_request_temperatures();
  delay(CONVERSION_DELAY_MS);  // Ждём завершения конверсии
  sensors_read_temperatures();
  
  // Применяем фильтр
  for (int i = 0; i < 4; i++) {
    if (sensors[i].found && sensors_is_valid(sensors[i].temp)) {
      // Медианный фильтр (упрощённо - скользящее среднее)
      filterSum[i] -= filterBuffer[i][filterIndex[i]];
      filterBuffer[i][filterIndex[i]] = sensors[i].temp;
      filterSum[i] += sensors[i].temp;
      filterIndex[i] = (filterIndex[i] + 1) % 5;
      
      sensors[i].temp = filterSum[i] / 5.0;
    }
  }
}

// ============================================================================
// ДОСТУП К ДАННЫМ
// ============================================================================
float sensors_get_temp(int idx) {
  if (idx < 0 || idx >= 4) return TEMP_NO_DATA;
  return sensors[idx].temp;
}

bool sensors_is_found(int idx) {
  if (idx < 0 || idx >= 4) return false;
  return sensors[idx].found;
}

float sensors_get_filtered(int idx) {
  return sensors_get_temp(idx);  // Пока просто температура, потом можно добавить калибровку
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================
bool sensors_is_valid(float temp) {
  if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) return false;
  if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) return false;
  if (temp <= TEMP_NO_DATA + 1.0f && temp >= TEMP_NO_DATA - 1.0f) return false;
  if (temp < -55.0f || temp > 125.0f) return false;
  return true;
}

void sensors_print_address(int idx) {
  if (idx < 0 || idx >= 4 || !sensors[idx].found) return;
  
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", sensors[idx].addr[i]);
  }
  Serial.println();
}