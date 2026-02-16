#ifndef EEPROM_SETTINGS_H
#define EEPROM_SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

// ============================================================================
// СТРУКТУРА НАСТРОЕК (ВСЕ ПАРАМЕТРЫ СИСТЕМЫ)
// ============================================================================
typedef struct {
  // Заголовок и версия
  uint16_t magic;           // Магическое число для проверки (0xAA55)
  uint8_t version;          // Версия структуры (для совместимости)
  
  // Калибровка
  float calibrationOffsets[4];  // Поправки для датчиков 0-3
  uint8_t referenceSensor;      // Датчик-эталон
  
  // Цветовые пороги
  float greenThreshold;     // Порог зелёный→жёлтый
  float yellowThreshold;    // Порог жёлтый→красный
  float hysteresis;         // Гистерезис
  
  // MP3 настройки (НОВОЕ)
  uint8_t mp3Volume;        // Громкость MP3 (0-30)
  
  // WiFi (для будущего)
  char wifiSSID[32];
  char wifiPassword[64];
  
  // MQTT (для будущего)
  char mqttServer[64];
  uint16_t mqttPort;
  char mqttUser[32];
  char mqttPassword[32];
  
  // Флаги
  bool calibrationEnabled;  // Включена ли калибровка
  bool wifiEnabled;         // Включён ли WiFi
  bool mqttEnabled;         // Включён ли MQTT
  
  // Резерв
  uint8_t reserved[32];     // На будущее
} SystemSettings_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void settings_init();                    // Загрузить или создать настройки по умолчанию
void settings_save();                     // Сохранить в EEPROM
void settings_reset();                    // Сбросить на defaults

// ============================================================================
// ДОСТУП К НАСТРОЙКАМ
// ============================================================================
SystemSettings_t* settings_get();         // Получить указатель на текущие настройки

// ============================================================================
// КАЛИБРОВКА
// ============================================================================
float settings_get_offset(int idx);       // Получить поправку для датчика
void settings_set_offset(int idx, float offset); // Установить поправку
void settings_set_reference(int idx);     // Установить эталонный датчик
int settings_get_reference();              // Получить эталонный датчик

// ============================================================================
// ЦВЕТОВЫЕ ПОРОГИ
// ============================================================================
float settings_get_green_threshold();
void settings_set_green_threshold(float value);
float settings_get_yellow_threshold();
void settings_set_yellow_threshold(float value);
float settings_get_hysteresis();
void settings_set_hysteresis(float value);

// ============================================================================
// MP3 ГРОМКОСТЬ (НОВЫЕ ФУНКЦИИ)
// ============================================================================
uint8_t settings_get_mp3_volume();
void settings_set_mp3_volume(uint8_t vol);

// ============================================================================
// СЛУЖЕБНЫЕ ФУНКЦИИ
// ============================================================================
void settings_print();                     // Вывести все настройки в Serial

#endif