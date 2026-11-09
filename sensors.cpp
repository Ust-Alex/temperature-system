/**
 * ============================================================================
 * ФАЙЛ: sensors.cpp
 * МОДУЛЬ УПРАВЛЕНИЯ ДАТЧИКАМИ DS18B20 (5 ШИН, 7 ДАТЧИКОВ)
 * ВЕРСИЯ: 5.0 (ДОБАВЛЕН ДАТЧИК ВОДА)
 * 
 * РАСПРЕДЕЛЕНИЕ ДАТЧИКОВ ПО ШИНАМ:
 * - Шина A (GPIO4)  → датчик гильзы (индекс 4) - БЕЗ привязки по адресу
 * - Шина B (GPIO16) → датчики стенок (индексы 1,2,3) - ПРИВЯЗКА по адресу
 * - Шина C (GPIO22) → датчик выхода (индекс 0) - БЕЗ привязки по адресу
 * - Шина D (GPIO21) → датчик куба (индекс 5) - БЕЗ привязки по адресу
 * - Шина E (GPIO17) → датчик вода (индекс 6) - БЕЗ привязки по адресу
 * 
 * ФИЛЬТРАЦИЯ:
 * - Скользящее среднее (буфер 5) → для гильзы (4) и выхода (0)
 * - Медианный фильтр (буфер 5) → для стенок (1,2,3), куба (5) и воды (6)
 * ============================================================================
 */

#include "sensors.h"
#include "system_config.h"
#include "globals.h"
#include "calibration_simple.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ДАННЫЕ ДЛЯ ФИЛЬТРОВ
// ============================================================================

// Буфер для скользящего среднего (только гильза и выход)
static float avgBuffer[7][5];
static int avgIndex[7] = {0};
static float avgSum[7] = {0};

// Буфер для медианного фильтра (стенки, куб, вода)
static float medBuffer[7][5];
static int medIndex[7] = {0};

// ============================================================================
// АДРЕСА ДАТЧИКОВ ДЛЯ ШИНЫ B (СТЕНКИ, ИНДЕКСЫ 1,2,3)
// ============================================================================
static const uint8_t KNOWN_ADDR_WALLS[3][8] = {
    { 0x28, 0xE0, 0x6C, 0x05, 0x47, 0x24, 0x0B, 0x17 }, // индекс 1: СТЕНКА 100см
    { 0x28, 0x6E, 0xE3, 0x41, 0x47, 0x24, 0x0B, 0xA4 }, // индекс 2: СТЕНКА 75см
    { 0x28, 0xEE, 0x0B, 0x60, 0x46, 0x24, 0x0B, 0xFE }  // индекс 3: СТЕНКА 50см
};

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: СОРТИРОВКА ДЛЯ МЕДИАНЫ
// ============================================================================
static void sort5(float* arr) {
    for (int i = 1; i < 5; i++) {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// ============================================================================
// МЕДИАННЫЙ ФИЛЬТР (ДЛЯ СТЕНОК, КУБА, ВОДЫ)
// ============================================================================
static float filterMedian(int sensorIdx, float newValue) {
    medBuffer[sensorIdx][medIndex[sensorIdx]] = newValue;
    medIndex[sensorIdx] = (medIndex[sensorIdx] + 1) % 5;
    
    float temp[5];
    for (int i = 0; i < 5; i++) {
        temp[i] = medBuffer[sensorIdx][i];
    }
    
    sort5(temp);
    return temp[2];
}

// ============================================================================
// ФИЛЬТР СКОЛЬЗЯЩЕГО СРЕДНЕГО (ДЛЯ ГИЛЬЗЫ И ВЫХОДА)
// ============================================================================
static float filterMovingAverage(int sensorIdx, float newValue) {
    avgSum[sensorIdx] -= avgBuffer[sensorIdx][avgIndex[sensorIdx]];
    avgBuffer[sensorIdx][avgIndex[sensorIdx]] = newValue;
    avgSum[sensorIdx] += newValue;
    avgIndex[sensorIdx] = (avgIndex[sensorIdx] + 1) % 5;
    
    return avgSum[sensorIdx] / 5.0f;
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ФИЛЬТРОВ
// ============================================================================
static void init_filters() {
    // Инициализация скользящего среднего (для гильзы и выхода)
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 5; j++) avgBuffer[i][j] = 18.0f;
        avgSum[i] = 90.0f;
        avgIndex[i] = 0;
    }
    
    // Инициализация медианного фильтра (для остальных)
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 5; j++) medBuffer[i][j] = 18.0f;
        medIndex[i] = 0;
    }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МОДУЛЯ ДАТЧИКОВ
// ============================================================================
void sensors_init() {
    // Запуск всех 5 шин
    sensorsA.begin();
    sensorsB.begin();
    sensorsC.begin();
    sensorsD.begin();
    sensorsE.begin();
    
    // Установка разрешения для всех шин
    sensorsA.setResolution(RESOLUTION);
    sensorsB.setResolution(RESOLUTION);
    sensorsC.setResolution(RESOLUTION);
    sensorsD.setResolution(RESOLUTION);
    sensorsE.setResolution(RESOLUTION);
    
    init_filters();
    sensors_scan_all();
    Serial.println("[SENSORS] OK (5 шин, 7 датчиков)");
}

// ============================================================================
// ПОИСК ДАТЧИКОВ НА ВСЕХ ШИНАХ
// ============================================================================
void sensors_scan_all() {
    // Сброс флагов
    for (int i = 0; i < 7; i++) sensors[i].found = false;

    DeviceAddress addr;
    
    // ============================================================
    // ШИНА C (GPIO22) — датчик ВЫХОД (индекс 0)
    // ============================================================
    if (sensorsC.getAddress(addr, 0)) {
        sensors[0].found = true;
        memcpy(sensors[0].addr, addr, 8);
        Serial.println("[SENSORS] ВЫХОД найден на шине C (GPIO22)");
    }
    
    // ============================================================
    // ШИНА B (GPIO16) — датчики стенок (индексы 1,2,3)
    // ============================================================
    int cnt = sensorsB.getDeviceCount();
    for (int i = 0; i < cnt && i < 3; i++) {
        if (!sensorsB.getAddress(addr, i)) continue;
        for (int j = 0; j < 3; j++) {
            if (memcmp(addr, KNOWN_ADDR_WALLS[j], 8) == 0) {
                int idx = j + 1; // индексы 1,2,3
                sensors[idx].found = true;
                memcpy(sensors[idx].addr, addr, 8);
                Serial.printf("[SENSORS] %s найден на шине B (GPIO16)\n", sensorNames[idx]);
                break;
            }
        }
    }
    
    // ============================================================
    // ШИНА A (GPIO4) — датчик ГИЛЬЗА (индекс 4)
    // ============================================================
    if (sensorsA.getAddress(addr, 0)) {
        sensors[4].found = true;
        memcpy(sensors[4].addr, addr, 8);
        Serial.println("[SENSORS] ГИЛЬЗА найдена на шине A (GPIO4)");
    }
    
    // ============================================================
    // ШИНА D (GPIO21) — датчик КУБ (индекс 5)
    // ============================================================
    if (sensorsD.getAddress(addr, 0)) {
        sensors[5].found = true;
        memcpy(sensors[5].addr, addr, 8);
        Serial.println("[SENSORS] КУБ найден на шине D (GPIO21)");
    }

    // ============================================================
    // ШИНА E (GPIO17) — датчик ВОДА (индекс 6)
    // ============================================================
    if (sensorsE.getAddress(addr, 0)) {
        sensors[6].found = true;
        memcpy(sensors[6].addr, addr, 8);
        Serial.println("[SENSORS] ВОДА найдена на шине E (GPIO17)");
    }

    // Диагностика
    int found = 0;
    for (int i = 0; i < 7; i++) if (sensors[i].found) found++;
    Serial.printf("[SENSORS] Найдено %d/7 датчиков\n", found);
    if (!sensors[4].found) Serial.println("[SENSORS] КРИТИЧНО: Нет гильзы!");
}

// ============================================================================
// ЗАПРОС ТЕМПЕРАТУР (ПАРАЛЛЕЛЬНО НА ВСЕХ ШИНАХ)
// ============================================================================
void sensors_request_temperatures() {
    sensorsA.requestTemperatures();
    sensorsB.requestTemperatures();
    sensorsC.requestTemperatures();
    sensorsD.requestTemperatures();
    sensorsE.requestTemperatures();
}

// ============================================================================
// ЧТЕНИЕ ТЕМПЕРАТУР СО ВСЕХ ШИН
// ============================================================================
void sensors_read_temperatures() {
    for (int i = 0; i < 7; i++) {
        if (!sensors[i].found) continue;
        
        // Выбираем правильную шину по индексу датчика
        DallasTemperature* bus = NULL;
        if (i == 4) bus = &sensorsA;       // гильза (шина A)
        else if (i == 0) bus = &sensorsC;  // выход (шина C)
        else if (i == 5) bus = &sensorsD;  // куб (шина D)
        else if (i == 6) bus = &sensorsE;  // вода (шина E)
        else bus = &sensorsB;              // стенки (шина B, индексы 1,2,3)
        
        float t = bus->getTempC(sensors[i].addr);
        
        if (t == DEVICE_DISCONNECTED_C || t < -55 || t > 125) {
            sensors[i].temp = (i == 4) ? TEMP_CRITICAL_LOST : TEMP_SENSOR_LOST;
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

    for (int i = 0; i < 7; i++) {
        if (!sensors[i].found || !isValidTemperature(sensors[i].temp)) continue;

        float filteredTemp;
        
        // ================================================================
        // ВЫБОР ФИЛЬТРА В ЗАВИСИМОСТИ ОТ ТИПА ДАТЧИКА
        // ================================================================
        if (i == 0 || i == 4) {
            // Выход и Гильза — скользящее среднее (плавное сглаживание)
            filteredTemp = filterMovingAverage(i, sensors[i].temp);
        } else {
            // Все остальные (стенки, куб, вода) — медианный фильтр
            filteredTemp = filterMedian(i, sensors[i].temp);
        }
        
        // Сохраняем отфильтрованное и калибруем
        sensors[i].temp = filteredTemp;
        sensors[i].temp = applyCalibration(i, sensors[i].temp);
    }
}

// ============================================================================
// ДОСТУП К ДАННЫМ
// ============================================================================
float sensors_get_temp(int idx) {
    return (idx >= 0 && idx < 7) ? sensors[idx].temp : TEMP_NO_DATA;
}

bool sensors_is_found(int idx) {
    return (idx >= 0 && idx < 7) ? sensors[idx].found : false;
}

float sensors_get_filtered(int idx) {
    return sensors_get_temp(idx);
}