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
// СТРУКТУРЫ ДАННЫХ (ДЕЛЬТА УДАЛЕНА)
// ============================================================================

typedef struct {
  float temps[4];           // Температуры датчиков
  uint8_t colors[4];        // Цвета для каждого датчика (не используется в текущей версии)
  uint8_t mode;             // Текущий режим: 0 = MODE1, 1 = MODE2
  bool needsRedraw;         // Флаг необходимости полной перерисовки
} SystemData_t;

typedef struct {
  bool found;               // Найден ли датчик
  float temp;               // Текущая температура (отфильтрованная, откалиброванная)
  float baseTemp;           // Базовая температура для калибровки (не используется)
  uint8_t addr[8];          // Уникальный адрес датчика 1-Wire
  float filterBuffer[5];    // Буфер фильтра скользящего среднего
  int filterIndex;          // Текущий индекс в буфере фильтра
  float filterSum;          // Сумма значений в буфере фильтра
  uint32_t lostTimer;       // Таймер потери датчика (не используется)
} Sensor_t;

typedef struct {
  uint8_t cmd;              // Команда MP3-плееру
  uint16_t param;           // Параметр команды (номер трека, громкость и т.д.)
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

// КЭШ ОТОБРАЖЕНИЯ (ТОЛЬКО ДЛЯ ТЕМПЕРАТУРЫ)
extern float lastDisplayTemps[4];      // Последние отображённые значения температур
extern String lastTimeString;          // Кэш времени для MODE1
extern String lastMode2TimeString;     // Кэш времени для MODE2
extern bool displayInitialized;        // Флаг инициализации дисплея

// Константы для дисплея
extern const char* sensorNames[4];
extern int bigFontHeight;
extern int deltaFontHeight;
extern int smallFontHeight;
extern int maxTempWidth;
extern const int displayYPositions[4];

// Энкодер и интерфейс
extern uint8_t systemState;

#endif