// temperature_system проба


#include "system_config.h"
#include "rtos_tasks.h"
#include "wifi_ap_module.h"

void setup() {
  initHardware();
  wifi_ap_setup();
  create_rtos_tasks();
}

void loop() {
  vTaskDelete(NULL);
  wifi_ap_loop();
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