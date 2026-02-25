/**
 * ============================================================================
 * @file sensors.cpp
 * @brief МОДУЛЬ УПРАВЛЕНИЯ ДАТЧИКАМИ DS18B20 (ОПТИМИЗИРОВАНО)
 * @version 2.1
 * 
 * ОСОБЕННОСТИ:
 * - Две независимые шины 1-Wire (A и B)
 * - Фильтр скользящего среднего (5 значений)
 * - Жёсткая привязка датчиков по адресам
 * - Автоматическое применение калибровки
 * ============================================================================
 */

#include "sensors.h"
#include "system_config.h"
#include "globals.h"
#include "calibration_simple.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ДАННЫЕ
// ============================================================================
static float filterBuffer[4][5];
static int filterIndex[4] = {0};
static float filterSum[4] = {0};

// Адреса датчиков для жёсткой привязки
static const uint8_t KNOWN_ADDR[4][8] = {
    { 0x28, 0xEE, 0x0B, 0x60, 0x46, 0x24, 0x0B, 0xFE }, // 0: 100см
    { 0x28, 0xE0, 0x6C, 0x05, 0x47, 0x24, 0x0B, 0x17 }, // 1: 75см
    { 0x28, 0x6E, 0xE3, 0x41, 0x47, 0x24, 0x0B, 0xA4 }, // 2: 50см
    { 0x28, 0x02, 0xD3, 0x34, 0x0F, 0x00, 0x00, 0x79 }  // 3: гильза
};

static const char* SENSOR_NAMES[4] = {
    "100см", "75см", "50см", "ГИЛЬЗА"
};

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
static void init_filter() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 5; j++) filterBuffer[i][j] = 18.0f;
        filterSum[i] = 90.0f;
        filterIndex[i] = 0;
    }
}

void sensors_init() {
    sensorsA.begin();
    sensorsB.begin();
    sensorsA.setResolution(RESOLUTION);
    sensorsB.setResolution(RESOLUTION);
    init_filter();
    sensors_scan_all();
    Serial.println("[SENSORS] OK");
}

// ============================================================================
// ПОИСК ДАТЧИКОВ
// ============================================================================
void sensors_scan_all() {
    // Сброс флагов
    for (int i = 0; i < 4; i++) sensors[i].found = false;

    // Поиск гильзы на шине A
    DeviceAddress addr;
    if (sensorsA.getAddress(addr, 0)) {
        if (memcmp(addr, KNOWN_ADDR[3], 8) == 0) {
            sensors[3].found = true;
            memcpy(sensors[3].addr, addr, 8);
        }
    }

    // Поиск стенок на шине B
    int cnt = sensorsB.getDeviceCount();
    for (int i = 0; i < cnt; i++) {
        if (!sensorsB.getAddress(addr, i)) continue;
        for (int j = 0; j < 3; j++) {
            if (memcmp(addr, KNOWN_ADDR[j], 8) == 0) {
                sensors[j].found = true;
                memcpy(sensors[j].addr, addr, 8);
                break;
            }
        }
    }

    // Диагностика (краткая)
    int found = 0;
    for (int i = 0; i < 4; i++) if (sensors[i].found) found++;
    Serial.printf("[SENSORS] Найдено %d/4 датчиков\n", found);
    if (!sensors[3].found) Serial.println("[SENSORS] КРИТИЧНО: Нет гильзы!");
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
        if (!sensors[i].found) continue;
        
        DallasTemperature* bus = (i == 3) ? &sensorsA : &sensorsB;
        float t = bus->getTempC(sensors[i].addr);
        
        if (t == DEVICE_DISCONNECTED_C || t < -55 || t > 125) {
            sensors[i].temp = TEMP_SENSOR_LOST;
        } else {
            sensors[i].temp = t;
        }
    }
}

// ============================================================================
// ПОЛНЫЙ ЦИКЛ: ЗАПРОС → ЧТЕНИЕ → ФИЛЬТР → КАЛИБРОВКА
// ============================================================================
void sensors_update_all() {
    sensors_request_temperatures();
    delay(CONVERSION_DELAY_MS);
    sensors_read_temperatures();

    for (int i = 0; i < 4; i++) {
        if (!sensors[i].found || !sensors_is_valid(sensors[i].temp)) continue;

        // Фильтр (скользящее среднее)
        filterSum[i] -= filterBuffer[i][filterIndex[i]];
        filterBuffer[i][filterIndex[i]] = sensors[i].temp;
        filterSum[i] += sensors[i].temp;
        filterIndex[i] = (filterIndex[i] + 1) % 5;
        
        // Сохраняем отфильтрованное и калибруем
        sensors[i].temp = filterSum[i] / 5.0;
        sensors[i].temp = applyCalibration(i, sensors[i].temp);
    }
}

// ============================================================================
// ДОСТУП К ДАННЫМ
// ============================================================================
float sensors_get_temp(int idx) {
    return (idx >= 0 && idx < 4) ? sensors[idx].temp : TEMP_NO_DATA;
}

bool sensors_is_found(int idx) {
    return (idx >= 0 && idx < 4) ? sensors[idx].found : false;
}

float sensors_get_filtered(int idx) {
    return sensors_get_temp(idx);
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================
bool sensors_is_valid(float temp) {
    if (temp <= TEMP_CRITICAL_LOST + 1.0f && temp >= TEMP_CRITICAL_LOST - 1.0f) return false;
    if (temp <= TEMP_SENSOR_LOST + 1.0f && temp >= TEMP_SENSOR_LOST - 1.0f) return false;
    if (temp <= TEMP_NO_DATA + 1.0f && temp >= TEMP_NO_DATA - 1.0f) return false;
    return (temp >= -55 && temp <= 125);
}

void sensors_print_address(int idx) {
    if (idx < 0 || idx >= 4 || !sensors[idx].found) return;
    for (int i = 0; i < 8; i++) {
        Serial.printf("%02X", sensors[idx].addr[i]);
        if (i < 7) Serial.print(":");
    }
}