#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DFRobotDFPlayerMini.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ============================================================================
// СТРУКТУРЫ ДАННЫХ (копируем из system_config.h или temperature_system.ino)
// ============================================================================

typedef struct {
  float temps[4];
  float deltas[4];
  uint8_t colors[4];
  uint8_t mode;
  bool needsRedraw;
} SystemData_t;

typedef struct {
  bool found;
  float temp;
  float baseTemp;
  uint8_t addr[8];
  float filterBuffer[5];
  int filterIndex;
  float filterSum;
  uint32_t lostTimer;
} Sensor_t;

typedef struct {
  uint8_t cmd;
  uint16_t param;
} Mp3Command_t;

// ============================================================================
// ОБЪЯВЛЕНИЯ ВСЕХ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ (extern)
// ============================================================================

// Объекты аппаратуры
extern TFT_eSPI tft;
extern OneWire oneWireA;
extern OneWire oneWireB;
extern DallasTemperature sensorsA;
extern DallasTemperature sensorsB;
extern HardwareSerial dfplayerSerial;
extern DFRobotDFPlayerMini myDFPlayer;

// Очереди и мьютексы
extern QueueHandle_t mp3CommandQueue;
extern QueueHandle_t dataQueue;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t eventQueue;

// Системные данные
extern SystemData_t sysData;
extern Sensor_t sensors[4];

// Флаги состояния
extern bool baseSaved;
extern bool systemInitialized;
extern bool criticalError;
extern bool forceDisplayRedraw;
extern bool mp3PlayerReady;

// Режимы и таймеры
extern uint8_t lastDisplayMode;
extern uint16_t lastGlobalBgColor;
extern float timeRefTemp;
extern uint32_t timeStartMs;
extern bool timeIsCounting;
extern float guildBaseTemp;
extern uint8_t guildColorState;

// Кэш отображения
extern float lastDisplayTemps[4];
extern float lastDisplayDeltas[4];

// Константы для дисплея
extern const char* sensorNames[4];
extern int bigFontHeight;
extern int deltaFontHeight;
extern int smallFontHeight;
extern int maxTempWidth;
extern int maxDeltaWidth;
extern const int displayYPositions[4];

// Энкодер и интерфейс
extern uint8_t systemState;

#endif