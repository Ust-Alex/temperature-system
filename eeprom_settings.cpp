#include "eeprom_settings.h"
#include "system_config.h"
#include "globals.h"
#include "mp3_player.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static SystemSettings_t currentSettings;
static bool settingsLoaded = false;

// ============================================================================
// ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ (ВЕРСИЯ 2)
// ============================================================================
static const SystemSettings_t DEFAULT_SETTINGS = {
  .magic = 0xAA55,
  .version = 2,
  .calibrationOffsets = { 0, 0, 0, 0 },
  .referenceSensor = 3,
  .greenThreshold = GREEN_TO_YELLOW_THRESHOLD,
  .yellowThreshold = YELLOW_TO_RED_THRESHOLD,
  .hysteresis = HYSTERESIS_VALUE,
  .mp3Volume = 15,
  .wifiSSID = "",
  .wifiPassword = "",
  .mqttServer = "",
  .mqttPort = 1883,
  .mqttUser = "",
  .mqttPassword = "",
  .calibrationEnabled = true,
  .wifiEnabled = false,
  .mqttEnabled = false,
  .reserved = { 0 }
};

// ============================================================================
// MP3 ГРОМКОСТЬ
// ============================================================================
uint8_t settings_get_mp3_volume() {
  return settings_get()->mp3Volume;
}

void settings_set_mp3_volume(uint8_t vol) {
  if (vol > 30) vol = 30;
  settings_get()->mp3Volume = vol;
  settings_save();

  Mp3Command_t volCmd = { MP3_CMD_SET_VOLUME, vol };
  sendMP3Command(volCmd);
  Serial.printf("[SETTINGS] Громкость MP3: %d/30\n", vol);
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void settings_init() {
  Serial.println("[SETTINGS] Инициализация...");
  EEPROM.begin(sizeof(SystemSettings_t) + 16);

  EEPROM.get(0, currentSettings);

  // Проверяем валидность (ожидаем версию 2)
  if (currentSettings.magic != 0xAA55 || currentSettings.version != 2) {
    Serial.println("[SETTINGS] Настройки не найдены или устарели. Загружаем defaults.");
    settings_reset();
  } else {
    Serial.println("[SETTINGS] Загружено из EEPROM.");
    settingsLoaded = true;
  }
  settings_print();
}

void settings_save() {
  currentSettings.magic = 0xAA55;
  currentSettings.version = 2;
  EEPROM.put(0, currentSettings);
  EEPROM.commit();
  Serial.println("[SETTINGS] Сохранено.");
}

void settings_reset() {
  Serial.println("[SETTINGS] Сброс к заводским.");
  memcpy(&currentSettings, &DEFAULT_SETTINGS, sizeof(SystemSettings_t));
  settings_save();
  settingsLoaded = true;
}

SystemSettings_t* settings_get() {
  if (!settingsLoaded) settings_init();
  return &currentSettings;
}

// ============================================================================
// КАЛИБРОВКА
// ============================================================================
bool settings_get_calibration_enabled() {
  return settings_get()->calibrationEnabled;
}
void settings_set_calibration_enabled(bool enabled) {
  settings_get()->calibrationEnabled = enabled;
  settings_save();
}

float settings_get_offset(int idx) {
  return (idx >= 0 && idx < 4) ? settings_get()->calibrationOffsets[idx] : 0;
}
void settings_set_offset(int idx, float offset) {
  if (idx >= 0 && idx < 4) {
    settings_get()->calibrationOffsets[idx] = offset;
    settings_save();
  }
}

int settings_get_reference() {
  return settings_get()->referenceSensor;
}
void settings_set_reference(int idx) {
  if (idx >= 0 && idx < 4) {
    settings_get()->referenceSensor = idx;
    settings_save();
  }
}

// ============================================================================
// ЦВЕТОВЫЕ ПОРОГИ
// ============================================================================
float settings_get_green_threshold() { return settings_get()->greenThreshold; }
void settings_set_green_threshold(float v) {
  if (v > 0 && v < 1.0) { settings_get()->greenThreshold = v; settings_save(); }
}
float settings_get_yellow_threshold() { return settings_get()->yellowThreshold; }
void settings_set_yellow_threshold(float v) {
  if (v > 0 && v < 1.0) { settings_get()->yellowThreshold = v; settings_save(); }
}
float settings_get_hysteresis() { return settings_get()->hysteresis; }
void settings_set_hysteresis(float v) {
  if (v > 0 && v < 0.1) { settings_get()->hysteresis = v; settings_save(); }
}

// ============================================================================
// ВЫВОД НАСТРОЕК
// ============================================================================
void settings_print() {
  SystemSettings_t* s = settings_get();
  Serial.println("\n" + String(50, '='));
  Serial.println("ТЕКУЩИЕ НАСТРОЙКИ СИСТЕМЫ");
  Serial.println(String(50, '='));
  Serial.printf("Версия: %d\n", s->version);
  Serial.println("\n--- Калибровка ---");
  Serial.printf("Эталон: %d\n", s->referenceSensor);
  for (int i = 0; i < 4; i++) Serial.printf("  Offset[%d]: %+.2f\n", i, s->calibrationOffsets[i]);
  Serial.printf("Активна: %s\n", s->calibrationEnabled ? "ДА" : "НЕТ");

  Serial.println("\n--- Цветовые пороги ---");
  Serial.printf("G→Y: %.3f  Y→R: %.3f  Hyst: %.3f\n", s->greenThreshold, s->yellowThreshold, s->hysteresis);

  Serial.println("\n--- WiFi ---");
  Serial.printf("SSID: %s\n", s->wifiSSID);
  Serial.printf("Включён: %s\n", s->wifiEnabled ? "ДА" : "НЕТ");

  Serial.println("\n--- MQTT ---");
  Serial.printf("Сервер: %s\n", s->mqttServer);
  Serial.printf("Порт: %d\n", s->mqttPort);
  Serial.printf("Включён: %s\n", s->mqttEnabled ? "ДА" : "НЕТ");
  Serial.println(String(50, '=') + "\n");
}