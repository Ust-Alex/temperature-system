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
// СТРУКТУРЫ ДАННЫХ (ДЛЯ 6 ДАТЧИКОВ)
// ============================================================================

typedef struct {
  float temps[6];           // Температуры датчиков (6 шт.)
  uint8_t colors[6];        // Цвета для каждого датчика
  uint8_t mode;             // Текущий режим: 0 = MODE1, 1 = MODE2
  bool needsRedraw;         // Флаг необходимости полной перерисовки
} SystemData_t;

typedef struct {
  bool found;               // Найден ли датчик
  float temp;               // Текущая температура
  float baseTemp;           // Базовая температура для калибровки
  uint8_t addr[8];          // Уникальный адрес датчика 1-Wire
  float filterBuffer[5];    // Буфер фильтра скользящего среднего
  int filterIndex;          // Текущий индекс в буфере фильтра
  float filterSum;          // Сумма значений в буфере фильтра
  uint32_t lostTimer;       // Таймер потери датчика
} Sensor_t;

typedef struct {
  uint8_t cmd;              // Команда MP3-плееру
  uint16_t param;           // Параметр команды
} Mp3Command_t;

// ============================================================================
// ОБЪЯВЛЕНИЯ ВСЕХ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ (extern)
// ============================================================================

// Объекты аппаратуры (4 шины)
extern TFT_eSPI tft;
extern OneWire oneWireA;
extern OneWire oneWireB;
extern OneWire oneWireC;
extern OneWire oneWireD;
extern DallasTemperature sensorsA;
extern DallasTemperature sensorsB;
extern DallasTemperature sensorsC;
extern DallasTemperature sensorsD;
extern HardwareSerial dfplayerSerial;
extern DFRobotDFPlayerMini myDFPlayer;

// Очереди и мьютексы
extern QueueHandle_t mp3CommandQueue;
extern QueueHandle_t dataQueue;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t eventQueue;

// Системные данные
extern SystemData_t sysData;
extern Sensor_t sensors[6];

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

// КЭШ ОТОБРАЖЕНИЯ
extern float lastDisplayTemps[6];
extern String lastTimeString;
extern String lastMode2TimeString;
extern bool displayInitialized;

// Константы для дисплея
extern const char* sensorNames[6];
extern int bigFontHeight;
extern int deltaFontHeight;
extern int smallFontHeight;
extern int maxTempWidth;

// ============================================================================
// ПАРАМЕТРЫ ОТРИСОВКИ ДАТЧИКОВ (ЯВНЫЕ КООРДИНАТЫ И ШРИФТЫ)
// ============================================================================
extern const int sensorX[6];
extern const int sensorY[6];
extern const int sensorFont[6];

// ============================================================================
// ПРЕДРАССЧИТАННЫЕ РАЗМЕРЫ ДЛЯ ОЧИСТКИ ОБЛАСТИ
// ============================================================================
extern int bigFontHeightClear;
extern int bigTempWidthClear;
extern int deltaFontHeightClear;
extern int deltaTempWidthClear;

// Энкодер и интерфейс
extern uint8_t systemState;

#endif