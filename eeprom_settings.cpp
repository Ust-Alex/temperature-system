/**
 * ============================================================================
 * @file eeprom_settings.cpp
 * @brief РЕАЛИЗАЦИЯ РАБОТЫ С EEPROM (НАСТРОЙКИ)
 * @version 2.0 (ДОБАВЛЕНЫ НАСТРОЙКИ WI-FI)
 * ============================================================================
 */

#include "eeprom_settings.h"
#include <EEPROM.h>

// ============================================================================
// ЛОКАЛЬНЫЕ ДАННЫЕ
// ============================================================================
static Settings_t settings;
static bool settingsLoaded = false;

// ============================================================================
// ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ
// ============================================================================
static void set_defaults() {
  settings.magic = SETTINGS_MAGIC;
  settings.version = 2;
  
  settings.calibEnabled = true;
  for (int i = 0; i < 6; i++) settings.offsets[i] = 0.0f;
  settings.referenceSensor = 0;
  
  settings.greenThreshold = 0.12f;
  settings.yellowThreshold = 0.22f;
  settings.hysteresis = 0.02f;
  
  settings.mp3Volume = 20;
  
  settings.wifiSSID[0] = '\0';
  settings.wifiPassword[0] = '\0';
  settings.wifiConfigured = false;
  
  settings.useStaticIP = false;
  for (int i = 0; i < 4; i++) {
    settings.staticIP[i] = 0;
    settings.staticGateway[i] = 0;
    settings.staticSubnet[i] = 0;
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void settings_init() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Читаем настройки из EEPROM
  EEPROM.get(0, settings);
  
  // Проверяем магическое число
  if (settings.magic != SETTINGS_MAGIC || settings.version != 2) {
    Serial.println("[SETTINGS] Настройки не найдены или устарели. Установка значений по умолчанию.");
    set_defaults();
    settings_save();
  } else {
    Serial.println("[SETTINGS] Настройки загружены из EEPROM.");
  }
  
  settingsLoaded = true;
  
  // Вывод текущих настроек Wi-Fi
  if (settings.wifiConfigured) {
    Serial.printf("[SETTINGS] Wi-Fi настроен: SSID=%s\n", settings.wifiSSID);
  } else {
    Serial.println("[SETTINGS] Wi-Fi не настроен.");
  }
}

void settings_save() {
  if (!settingsLoaded) return;
  EEPROM.put(0, settings);
  EEPROM.commit();
  Serial.println("[SETTINGS] Настройки сохранены в EEPROM.");
}

void settings_reset() {
  set_defaults();
  settings_save();
  Serial.println("[SETTINGS] Настройки сброшены к значениям по умолчанию.");
}

// ============================================================================
// КАЛИБРОВКА
// ============================================================================
bool settings_get_calibration_enabled() {
  return settings.calibEnabled;
}

void settings_set_calibration_enabled(bool enabled) {
  settings.calibEnabled = enabled;
}

float settings_get_offset(int idx) {
  if (idx < 0 || idx >= 6) return 0.0f;
  return settings.offsets[idx];
}

void settings_set_offset(int idx, float offset) {
  if (idx < 0 || idx >= 6) return;
  settings.offsets[idx] = offset;
}

int settings_get_reference() {
  return settings.referenceSensor;
}

void settings_set_reference(int idx) {
  if (idx < 0 || idx >= 6) return;
  settings.referenceSensor = idx;
}

// ============================================================================
// ПОРОГИ
// ============================================================================
float settings_get_green_threshold() {
  return settings.greenThreshold;
}

void settings_set_green_threshold(float value) {
  settings.greenThreshold = value;
}

float settings_get_yellow_threshold() {
  return settings.yellowThreshold;
}

void settings_set_yellow_threshold(float value) {
  settings.yellowThreshold = value;
}

float settings_get_hysteresis() {
  return settings.hysteresis;
}

void settings_set_hysteresis(float value) {
  settings.hysteresis = value;
}

// ============================================================================
// ГРОМКОСТЬ MP3
// ============================================================================
uint8_t settings_get_mp3_volume() {
  return settings.mp3Volume;
}

void settings_set_mp3_volume(uint8_t volume) {
  if (volume > 30) volume = 30;
  settings.mp3Volume = volume;
}

// ============================================================================
// НОВЫЕ ФУНКЦИИ ДЛЯ НАСТРОЕК WI-FI
// ============================================================================
bool settings_has_wifi() {
  return settings.wifiConfigured && strlen(settings.wifiSSID) > 0;
}

String settings_get_ssid() {
  return String(settings.wifiSSID);
}

String settings_get_password() {
  return String(settings.wifiPassword);
}

void settings_save_wifi(const char* ssid, const char* password) {
  strncpy(settings.wifiSSID, ssid, sizeof(settings.wifiSSID) - 1);
  settings.wifiSSID[sizeof(settings.wifiSSID) - 1] = '\0';
  
  strncpy(settings.wifiPassword, password, sizeof(settings.wifiPassword) - 1);
  settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0';
  
  settings.wifiConfigured = true;
  settings_save();
  Serial.printf("[SETTINGS] Wi-Fi сохранён: SSID=%s\n", settings.wifiSSID);
}

void settings_clear_wifi() {
  settings.wifiSSID[0] = '\0';
  settings.wifiPassword[0] = '\0';
  settings.wifiConfigured = false;
  settings_save();
  Serial.println("[SETTINGS] Настройки Wi-Fi очищены.");
}

// ============================================================================
// СТАТИЧЕСКИЙ IP
// ============================================================================
bool settings_get_static_ip(uint8_t* ip, uint8_t* gateway, uint8_t* subnet) {
  if (!settings.useStaticIP) return false;
  memcpy(ip, settings.staticIP, 4);
  memcpy(gateway, settings.staticGateway, 4);
  memcpy(subnet, settings.staticSubnet, 4);
  return true;
}

void settings_set_static_ip(uint8_t* ip, uint8_t* gateway, uint8_t* subnet) {
  memcpy(settings.staticIP, ip, 4);
  memcpy(settings.staticGateway, gateway, 4);
  memcpy(settings.staticSubnet, subnet, 4);
  settings.useStaticIP = true;
}