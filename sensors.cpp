/**
 * ============================================================================
 * ФАЙЛ: sensors.cpp
 * МОДУЛЬ УПРАВЛЕНИЯ ДАТЧИКАМИ DS18B20 (4 ШИНЫ, 6 ДАТЧИКОВ)
 * ВЕРСИЯ: 4.1 (МЕДИАННЫЙ ФИЛЬТР ДЛЯ ВЫХОДА И КУБА)
 * 
 * РАСПРЕДЕЛЕНИЕ ДАТЧИКОВ ПО ШИНАМ:
 * - Шина A (GPIO4)  → датчик гильзы (индекс 4) - БЕЗ привязки по адресу
 * - Шина B (GPIO16) → датчики стенок (индексы 1,2,3) - ПРИВЯЗКА по адресу
 * - Шина C (GPIO21) → датчик выхода (индекс 0) - БЕЗ привязки по адресу
 * - Шина D (GPIO22) → датчик куба (индекс 5) - БЕЗ привязки по адресу
 * 
 * ФИЛЬТРАЦИЯ:
 * - Скользящее среднее (буфер 5) → для гильзы (4)
 * - Медианный фильтр (буфер 5) → для стенок (1,2,3), выхода (0), куба (5)
 * ============================================================================
 */

#include "sensors.h"
#include "system_config.h"
#include "globals.h"
#include "calibration_simple.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ДАННЫЕ ДЛЯ ФИЛЬТРОВ
// ============================================================================

// Буфер для скользящего среднего (только гильза, индекс 4)
static float avgBuffer[6][5];
static int avgIndex[6] = {0};
static float avgSum[6] = {0};

// Буфер для медианного фильтра (стенки 1,2,3 + выход 0 + куб 5)
static float medBuffer[6][5];  // теперь для всех, кроме гильзы
static int medIndex[6] = {0};

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
    // Простая сортировка вставками для 5 элементов
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
// МЕДИАННЫЙ ФИЛЬТР (ДЛЯ ВСЕХ, КРОМЕ ГИЛЬЗЫ)
// ============================================================================
static float filterMedian(int sensorIdx, float newValue) {
    // Записываем новое значение в буфер
    medBuffer[sensorIdx][medIndex[sensorIdx]] = newValue;
    medIndex[sensorIdx] = (medIndex[sensorIdx] + 1) % 5;
    
    // Копируем буфер во временный массив для сортировки
    float temp[5];
    for (int i = 0; i < 5; i++) {
        temp[i] = medBuffer[sensorIdx][i];
    }
    
    // Сортируем и возвращаем медиану (элемент с индексом 2)
    sort5(temp);
    return temp[2];
}

// ============================================================================
// ФИЛЬТР СКОЛЬЗЯЩЕГО СРЕДНЕГО (ТОЛЬКО ДЛЯ ГИЛЬЗЫ, ИНДЕКС 4)
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
    // Инициализация скользящего среднего (только для гильзы)
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 5; j++) avgBuffer[i][j] = 18.0f;
        avgSum[i] = 90.0f;
        avgIndex[i] = 0;
    }
    
    // Инициализация медианного фильтра (для всех, кроме гильзы)
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 5; j++) medBuffer[i][j] = 18.0f;
        medIndex[i] = 0;
    }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МОДУЛЯ ДАТЧИКОВ
// ============================================================================
void sensors_init() {
    // Запуск всех 4 шин
    sensorsA.begin();
    sensorsB.begin();
    sensorsC.begin();
    sensorsD.begin();
    
    // Установка разрешения для всех шин
    sensorsA.setResolution(RESOLUTION);
    sensorsB.setResolution(RESOLUTION);
    sensorsC.setResolution(RESOLUTION);
    sensorsD.setResolution(RESOLUTION);
    
    init_filters();
    sensors_scan_all();
    Serial.println("[SENSORS] OK (4 шины, 6 датчиков)");
}

// ============================================================================
// ПОИСК ДАТЧИКОВ НА ВСЕХ ШИНАХ
// ============================================================================
void sensors_scan_all() {
    // Сброс флагов
    for (int i = 0; i < 6; i++) sensors[i].found = false;

    DeviceAddress addr;
    
    // ============================================================
    // ШИНА C (GPIO21) — датчик ВЫХОД (индекс 0)
    // БЕЗ привязки по адресу — берём первый датчик на шине
    // ============================================================
    if (sensorsC.getAddress(addr, 0)) {
        sensors[0].found = true;
        memcpy(sensors[0].addr, addr, 8);
        Serial.println("[SENSORS] ВЫХОД найден на шине C (GPIO21)");
    }
    
    // ============================================================
    // ШИНА B (GPIO16) — датчики стенок (индексы 1,2,3)
    // ПРИВЯЗКА по фиксированным адресам
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
    // БЕЗ привязки по адресу — берём первый датчик на шине
    // ============================================================
    if (sensorsA.getAddress(addr, 0)) {
        sensors[4].found = true;
        memcpy(sensors[4].addr, addr, 8);
        Serial.println("[SENSORS] ГИЛЬЗА найдена на шине A (GPIO4)");
    }
    
    // ============================================================
    // ШИНА D (GPIO22) — датчик КУБ (индекс 5)
    // БЕЗ привязки по адресу — берём первый датчик на шине
    // ============================================================
    if (sensorsD.getAddress(addr, 0)) {
        sensors[5].found = true;
        memcpy(sensors[5].addr, addr, 8);
        Serial.println("[SENSORS] КУБ найден на шине D (GPIO22)");
    }

    // Диагностика
    int found = 0;
    for (int i = 0; i < 6; i++) if (sensors[i].found) found++;
    Serial.printf("[SENSORS] Найдено %d/6 датчиков\n", found);
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
}

// ============================================================================
// ЧТЕНИЕ ТЕМПЕРАТУР СО ВСЕХ ШИН
// ============================================================================
void sensors_read_temperatures() {
    for (int i = 0; i < 6; i++) {
        if (!sensors[i].found) continue;
        
        // Выбираем правильную шину по индексу датчика
        DallasTemperature* bus = NULL;
        if (i == 4) bus = &sensorsA;       // гильза (шина A)
        else if (i == 0) bus = &sensorsC;  // выход (шина C)
        else if (i == 5) bus = &sensorsD;  // куб (шина D)
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

    for (int i = 0; i < 6; i++) {
        if (!sensors[i].found || !isValidTemperature(sensors[i].temp)) continue;

        float filteredTemp;
        
        // ================================================================
        // ВЫБОР ФИЛЬТРА В ЗАВИСИМОСТИ ОТ ТИПА ДАТЧИКА
        // ================================================================
        if (i == 4) {
            // Гильза — скользящее среднее (плавное сглаживание)
            filteredTemp = filterMovingAverage(i, sensors[i].temp);
        } else {
            // Все остальные (стенки, выход, куб) — медианный фильтр
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
    return (idx >= 0 && idx < 6) ? sensors[idx].temp : TEMP_NO_DATA;
}

bool sensors_is_found(int idx) {
    return (idx >= 0 && idx < 6) ? sensors[idx].found : false;
}

float sensors_get_filtered(int idx) {
    return sensors_get_temp(idx);
}