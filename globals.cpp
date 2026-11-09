/**
 * ============================================================================
 * ФАЙЛ: globals.cpp
 * ОПРЕДЕЛЕНИЯ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ СИСТЕМЫ
 * ВЕРСИЯ: 3.1 (ДЛЯ 7 ДАТЧИКОВ, 5 ШИН)
 * ============================================================================
 */

#include "globals.h"
#include "system_config.h"

// ============================================================================
// ОБЪЕКТЫ АППАРАТУРЫ (5 ШИН)
// ============================================================================
TFT_eSPI tft;
OneWire oneWireA(ONE_WIRE_BUS_A);
OneWire oneWireB(ONE_WIRE_BUS_B);
OneWire oneWireC(ONE_WIRE_BUS_C);
OneWire oneWireD(ONE_WIRE_BUS_D);
OneWire oneWireE(ONE_WIRE_BUS_E);
DallasTemperature sensorsA(&oneWireA);
DallasTemperature sensorsB(&oneWireB);
DallasTemperature sensorsC(&oneWireC);
DallasTemperature sensorsD(&oneWireD);
DallasTemperature sensorsE(&oneWireE);
HardwareSerial dfplayerSerial(2);
DFRobotDFPlayerMini myDFPlayer;

// ============================================================================
// ОЧЕРЕДИ И МЬЮТЕКСЫ FREERTOS
// ============================================================================
QueueHandle_t mp3CommandQueue = NULL;
QueueHandle_t dataQueue = NULL;
SemaphoreHandle_t dataMutex = NULL;
QueueHandle_t eventQueue = NULL;

// ============================================================================
// СИСТЕМНЫЕ ДАННЫЕ
// ============================================================================
SystemData_t sysData;
Sensor_t sensors[7] = {0};

// ============================================================================
// ФЛАГИ СОСТОЯНИЯ СИСТЕМЫ
// ============================================================================
bool baseSaved = false;
bool systemInitialized = false;
bool criticalError = false;
bool forceDisplayRedraw = false;
bool mp3PlayerReady = false;

// ============================================================================
// РЕЖИМЫ И ТАЙМЕРЫ
// ============================================================================
uint8_t lastDisplayMode = 0xFF;
uint16_t lastGlobalBgColor = 0xFFFF;
float timeRefTemp = 0.0f;
uint32_t timeStartMs = 0;
bool timeIsCounting = false;
float guildBaseTemp = 0.0f;
uint8_t guildColorState = 0;

// ============================================================================
// КЭШ ОТОБРАЖЕНИЯ
// ============================================================================
float lastDisplayTemps[7] = {-1000.0f, -1000.0f, -1000.0f, -1000.0f, -1000.0f, -1000.0f, -1000.0f};
String lastTimeString = "";
String lastMode2TimeString = "";
bool displayInitialized = false;

// ============================================================================
// КОНСТАНТЫ ДЛЯ ДИСПЛЕЯ
// ============================================================================
const char* sensorNames[7] = {
    "ВЫХОД",
    "СТЕНКА 100см",
    "СТЕНКА 75см",
    "СТЕНКА 50см",
    "ГИЛЬЗА",
    "КУБ",
    "ВОДА"
};

int bigFontHeight = 0;
int deltaFontHeight = 0;
int smallFontHeight = 0;
int maxTempWidth = 0;

// ============================================================================
// ПАРАМЕТРЫ ОТРИСОВКИ ДАТЧИКОВ (7 ДАТЧИКОВ)
// ============================================================================
const int sensorX[7] = {
    10,   // датчик 0 (ВЫХОД)
    170,  // датчик 1 (СТЕНКА 100см)
    170,  // датчик 2 (СТЕНКА 75см)
    170,  // датчик 3 (СТЕНКА 50см)
    10,   // датчик 4 (ГИЛЬЗА)
    10,   // датчик 5 (КУБ)
    10    // датчик 6 (ВОДА)
};

const int sensorY[7] = {
    65,    // датчик 0 (ВЫХОД)
    85,   // датчик 1 (СТЕНКА 100см)
    116,  // датчик 2 (СТЕНКА 75см)
    147,  // датчик 3 (СТЕНКА 50см)
    125,  // датчик 4 (ГИЛЬЗА)
    185,  // датчик 5 (КУБ)
    5     // датчик 6 (ВОДА)
};

const int sensorFont[7] = {
    FONT_BIG,     // датчик 0 (ВЫХОД)
    FONT_DELTA,   // датчик 1 (СТЕНКА 100см)
    FONT_DELTA,   // датчик 2 (СТЕНКА 75см)
    FONT_DELTA,   // датчик 3 (СТЕНКА 50см)
    FONT_BIG,     // датчик 4 (ГИЛЬЗА)
    FONT_BIG,     // датчик 5 (КУБ)
    FONT_BIG      // датчик 6 (ВОДА)
};

// ============================================================================
// ПРЕДРАССЧИТАННЫЕ РАЗМЕРЫ (ЗАПОЛНЯЮТСЯ В hardware_control.cpp)
// ============================================================================
int bigFontHeightClear = 0;
int bigTempWidthClear = 0;
int deltaFontHeightClear = 0;
int deltaTempWidthClear = 0;

// ============================================================================
// ЭНКОДЕР И УПРАВЛЕНИЕ ИНТЕРФЕЙСОМ
// ============================================================================
uint8_t systemState = 0;