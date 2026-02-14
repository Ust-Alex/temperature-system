#include "globals.h"
#include "system_config.h"

// ============================================================================
// ОПРЕДЕЛЕНИЯ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ
// ============================================================================

// Объекты аппаратуры
TFT_eSPI tft;
OneWire oneWireA(ONE_WIRE_BUS_A);
OneWire oneWireB(ONE_WIRE_BUS_B);
DallasTemperature sensorsA(&oneWireA);
DallasTemperature sensorsB(&oneWireB);
HardwareSerial dfplayerSerial(2);
DFRobotDFPlayerMini myDFPlayer;

// Очереди и мьютексы
QueueHandle_t mp3CommandQueue = NULL;
QueueHandle_t dataQueue = NULL;
SemaphoreHandle_t dataMutex = NULL;
QueueHandle_t eventQueue = NULL;

// Системные данные
SystemData_t sysData;
Sensor_t sensors[4] = {0};

// Флаги состояния
bool baseSaved = false;
bool systemInitialized = false;
bool criticalError = false;
bool forceDisplayRedraw = false;
bool mp3PlayerReady = false;

// Режимы и таймеры
uint8_t lastDisplayMode = 0xFF;
uint16_t lastGlobalBgColor = 0xFFFF;
float timeRefTemp = 0.0f;
uint32_t timeStartMs = 0;
bool timeIsCounting = false;
float guildBaseTemp = 0.0f;
uint8_t guildColorState = 0;

// Кэш отображения
float lastDisplayTemps[4] = { 0 };
float lastDisplayDeltas[4] = { 0 };

// Константы для дисплея
const char* sensorNames[4] = {
  "СТЕНКА 100см",
  "СТЕНКА 75см",
  "СТЕНКА 50cm",
  "ГИЛЬЗА"
};

int bigFontHeight = 0;
int deltaFontHeight = 0;
int smallFontHeight = 0;
int maxTempWidth = 0;
int maxDeltaWidth = 0;
const int displayYPositions[4] = { 0, 60, 120, 180 };

// Энкодер и интерфейс
uint8_t systemState = 0;