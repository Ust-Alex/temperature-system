#include "eeprom_settings.h"
#include "system_config.h"
#include "globals.h"

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static SystemSettings_t currentSettings;
static bool settingsLoaded = false;

// ============================================================================
// ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ
// ============================================================================
static const SystemSettings_t DEFAULT_SETTINGS = {
  .magic = 0xAA55,
  .version = 1,
  .calibrationOffsets = {0, 0, 0, 0},
  .referenceSensor = 2,  // 50см по умолчанию
  .greenThreshold = GREEN_TO_YELLOW_THRESHOLD,
  .yellowThreshold = YELLOW_TO_RED_THRESHOLD,
  .hysteresis = HYSTERESIS_VALUE,
  .wifiSSID = "",
  .wifiPassword = "",
  .mqttServer = "",
  .mqttPort = 1883,
  .mqttUser = "",
  .mqttPassword = "",
  .calibrationEnabled = true,
  .wifiEnabled = false,
  .mqttEnabled = false,
  .reserved = {0}
};

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void settings_init() {
  Serial.println("[SETTINGS] Инициализация...");
  
  EEPROM.begin(sizeof(SystemSettings_t) + 16);  // Немного с запасом
  
  // Читаем из EEPROM
  EEPROM.get(0, currentSettings);
  
  // Проверяем валидность
  if (currentSettings.magic != 0xAA55 || currentSettings.version != 1) {
    Serial.println("[SETTINGS] Настройки не найдены или устарели. Загружаем defaults.");
    settings_reset();
  } else {
    Serial.println("[SETTINGS] Настройки загружены из EEPROM.");
    settingsLoaded = true;
  }
  
  // Применяем к глобальным переменным
  // (пока просто выводим)
  settings_print();
}

void settings_save() {
  Serial.println("[SETTINGS] Сохранение...");
  
  currentSettings.magic = 0xAA55;
  currentSettings.version = 1;
  
  EEPROM.put(0, currentSettings);
  EEPROM.commit();
  
  Serial.println("[SETTINGS] Сохранено.");
}

void settings_reset() {
  Serial.println("[SETTINGS] Сброс к заводским настройкам.");
  
  memcpy(&currentSettings, &DEFAULT_SETTINGS, sizeof(SystemSettings_t));
  settings_save();
  settingsLoaded = true;
}

// ============================================================================
// ДОСТУП К НАСТРОЙКАМ
// ============================================================================
SystemSettings_t* settings_get() {
  if (!settingsLoaded) {
    settings_init();
  }
  return &currentSettings;
}

// ============================================================================
// КАЛИБРОВКА
// ============================================================================
float settings_get_offset(int idx) {
  if (idx < 0 || idx >= 4) return 0;
  return settings_get()->calibrationOffsets[idx];
}

void settings_set_offset(int idx, float offset) {
  if (idx < 0 || idx >= 4) return;
  settings_get()->calibrationOffsets[idx] = offset;
  settings_save();
}

void settings_set_reference(int idx) {
  if (idx < 0 || idx >= 4) return;
  settings_get()->referenceSensor = idx;
  settings_save();
}

int settings_get_reference() {
  return settings_get()->referenceSensor;
}

// ============================================================================
// ЦВЕТОВЫЕ ПОРОГИ
// ============================================================================
float settings_get_green_threshold() {
  return settings_get()->greenThreshold;
}

void settings_set_green_threshold(float value) {
  if (value > 0 && value < 1.0) {
    settings_get()->greenThreshold = value;
    settings_save();
  }
}

float settings_get_yellow_threshold() {
  return settings_get()->yellowThreshold;
}

void settings_set_yellow_threshold(float value) {
  if (value > 0 && value < 1.0) {
    settings_get()->yellowThreshold = value;
    settings_save();
  }
}

float settings_get_hysteresis() {
  return settings_get()->hysteresis;
}

void settings_set_hysteresis(float value) {
  if (value > 0 && value < 0.1) {
    settings_get()->hysteresis = value;
    settings_save();
  }
}

// ============================================================================
// СЛУЖЕБНЫЕ ФУНКЦИИ
// ============================================================================
void settings_print() {
  SystemSettings_t* s = settings_get();
  
  Serial.println("\n" + String(50, '='));
  Serial.println("ТЕКУЩИЕ НАСТРОЙКИ СИСТЕМЫ");
  Serial.println(String(50, '='));
  
  Serial.printf("Версия: %d\n", s->version);
  Serial.printf("Магия: 0x%04X\n", s->magic);
  
  Serial.println("\n--- Калибровка ---");
  Serial.printf("Эталонный датчик: %d\n", s->referenceSensor);
  for (int i = 0; i < 4; i++) {
    Serial.printf("  Offset[%d]: %+.2f°C\n", i, s->calibrationOffsets[i]);
  }
  Serial.printf("Калибровка: %s\n", s->calibrationEnabled ? "ВКЛ" : "ВЫКЛ");
  
  Serial.println("\n--- Цветовые пороги ---");
  Serial.printf("Зелёный→Жёлтый: %.3f°C\n", s->greenThreshold);
  Serial.printf("Жёлтый→Красный: %.3f°C\n", s->yellowThreshold);
  Serial.printf("Гистерезис: %.3f°C\n", s->hysteresis);
  
  Serial.println("\n--- WiFi (будущее) ---");
  Serial.printf("SSID: %s\n", s->wifiSSID);
  Serial.printf("Включён: %s\n", s->wifiEnabled ? "ДА" : "НЕТ");
  
  Serial.println("\n--- MQTT (будущее) ---");
  Serial.printf("Сервер: %s\n", s->mqttServer);
  Serial.printf("Порт: %d\n", s->mqttPort);
  Serial.printf("Включён: %s\n", s->mqttEnabled ? "ДА" : "НЕТ");
  
  Serial.println(String(50, '=') + "\n");
}