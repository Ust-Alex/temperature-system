/**
 * ============================================================================
 * @file eeprom_settings.h
 * @brief ЗАГОЛОВОЧНЫЙ ФАЙЛ ДЛЯ РАБОТЫ С EEPROM (НАСТРОЙКИ)
 * @version 2.0 (ДОБАВЛЕНЫ НАСТРОЙКИ WI-FI)
 * 
 * ХРАНИМЫЕ НАСТРОЙКИ:
 * - Режим калибровки (вкл/выкл)
 * - Коэффициенты калибровки (offset) для 6 датчиков
 * - Эталонный датчик для калибровки
 * - Пороги (зелёный/жёлтый/красный)
 * - Гистерезис
 * - Громкость MP3
 * - Wi-Fi настройки: SSID, пароль
 * - Флаг наличия настроек Wi-Fi
 * ============================================================================
 */

#ifndef EEPROM_SETTINGS_H
#define EEPROM_SETTINGS_H

#include <Arduino.h>

// ============================================================================
// КОНСТАНТЫ EEPROM
// ============================================================================
#define EEPROM_SIZE 512
#define SETTINGS_MAGIC 0x55AA  // Магическое число для проверки валидности

// ============================================================================
// СТРУКТУРА НАСТРОЕК
// ============================================================================
typedef struct {
  uint16_t magic;           // Магическое число (проверка валидности)
  uint8_t version;          // Версия настроек
  
  // Калибровка
  bool calibEnabled;        // Включена ли калибровка
  float offsets[6];         // Смещения для 6 датчиков
  uint8_t referenceSensor;  // Индекс эталонного датчика (0-5)
  
  // Пороги
  float greenThreshold;
  float yellowThreshold;
  float hysteresis;
  
  // Громкость MP3
  uint8_t mp3Volume;
  
  // Wi-Fi настройки
  char wifiSSID[32];        // Имя сети (макс. 32 символа)
  char wifiPassword[64];    // Пароль (макс. 64 символа)
  bool wifiConfigured;      // Флаг: есть ли сохранённые настройки Wi-Fi
  
  // Статический IP (опционально)
  bool useStaticIP;
  uint8_t staticIP[4];
  uint8_t staticGateway[4];
  uint8_t staticSubnet[4];
} Settings_t;

// ============================================================================
// ОСНОВНЫЕ ФУНКЦИИ
// ============================================================================
void settings_init();                    // Инициализация EEPROM и загрузка настроек
void settings_save();                    // Сохранение настроек в EEPROM
void settings_reset();                   // Сброс настроек к значениям по умолчанию

// Калибровка
bool settings_get_calibration_enabled();
void settings_set_calibration_enabled(bool enabled);
float settings_get_offset(int idx);
void settings_set_offset(int idx, float offset);
int settings_get_reference();
void settings_set_reference(int idx);

// Пороги
float settings_get_green_threshold();
void settings_set_green_threshold(float value);
float settings_get_yellow_threshold();
void settings_set_yellow_threshold(float value);
float settings_get_hysteresis();
void settings_set_hysteresis(float value);

// Громкость MP3
uint8_t settings_get_mp3_volume();
void settings_set_mp3_volume(uint8_t volume);

// ============================================================================
// НОВЫЕ ФУНКЦИИ ДЛЯ НАСТРОЕК WI-FI
// ============================================================================
bool settings_has_wifi();                // Есть ли сохранённые настройки Wi-Fi
String settings_get_ssid();              // Получить SSID
String settings_get_password();          // Получить пароль
void settings_save_wifi(const char* ssid, const char* password);  // Сохранить настройки
void settings_clear_wifi();              // Очистить настройки Wi-Fi

// Статический IP
bool settings_get_static_ip(uint8_t* ip, uint8_t* gateway, uint8_t* subnet);
void settings_set_static_ip(uint8_t* ip, uint8_t* gateway, uint8_t* subnet);

#endif // EEPROM_SETTINGS_H