// temperature_system.ino

#include "system_config.h"
#include "rtos_tasks.h"

void setup() {
    initHardware();
    
    // Дополнительная инициализация (после initHardware)
    Serial.println("\n[INIT] Проверка системы...");
    
    // Создание задач FreeRTOS
    create_rtos_tasks();
    
    // Запуск таймера для периодических проверок
    Serial.println("[INIT] Система запущена. Мониторинг начат.");
}

void loop() {
    // Основной цикл пустой - всё работает в задачах FreeRTOS
    // Периодически проверяем общее состояние системы
    
    static uint32_t lastSystemCheck = 0;
    uint32_t currentMillis = millis();
    
    // Проверка каждые 5 минут
    if (currentMillis - lastSystemCheck > 300000) {
        Serial.println("\n[SYSTEM CHECK] ========================");
        Serial.printf("Время работы: %lu минут\n", currentMillis / 60000);
        Serial.printf("Режим: %d\n", sysData.mode);
        Serial.printf("Датчик гильзы: %s\n", sensors[3].found ? "OK" : "LOST");
        Serial.printf("Ошибка: %s\n", criticalError ? "ДА" : "нет");
        Serial.printf("Очередь данных: %s\n", dataQueue ? "создана" : "нет");
        Serial.printf("Мьютекс: %s\n", dataMutex ? "создан" : "нет");
        Serial.println("====================================\n");
        
        lastSystemCheck = currentMillis;
    }
    
    // Короткая пауза
    vTaskDelay(pdMS_TO_TICKS(1000));
}

TFT_eSPI tft;
OneWire oneWireA(ONE_WIRE_BUS_A);
OneWire oneWireB(ONE_WIRE_BUS_B);
DallasTemperature sensorsA(&oneWireA);
DallasTemperature sensorsB(&oneWireB);

SystemData_t sysData;
Sensor_t sensors[4];
QueueHandle_t dataQueue = NULL;
SemaphoreHandle_t dataMutex;

bool baseSaved = false;
bool systemInitialized = false;
bool criticalError = false;
bool forceDisplayRedraw = true;
uint8_t lastDisplayMode = 0xFF;
uint16_t lastGlobalBgColor = 0xFFFF;

float timeRefTemp = 0.0f;
uint32_t timeStartMs = 0;
bool timeIsCounting = false;

float guildBaseTemp = 0.0f;
uint8_t guildColorState = 0;

float lastDisplayTemps[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };
float lastDisplayDeltas[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };

const char* sensorNames[4] = { "Стенка 100см", "Стенка 75см", "Стенка 50см", "Гильза 25см" };

int bigFontHeight = 0;
int deltaFontHeight = 0;
int smallFontHeight = 0;
int maxTempWidth = 0;
int maxDeltaWidth = 0;

const int displayYPositions[4] = { 0, 60, 120, 180 };