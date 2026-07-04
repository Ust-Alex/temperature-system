/**
 * ============================================================================
 * @file mp3_player.cpp
 * @brief МОДУЛЬ УПРАВЛЕНИЯ DFPLAYER MINI (MP3-ПРОИГРЫВАТЕЛЕМ)
 * 
 * @version 2.4 (ДОБАВЛЕНА ОТПРАВКА ЗВУКОВЫХ КОМАНД В ВЕБ-ИНТЕРФЕЙС)
 * 
 * ОСОБЕННОСТИ:
 * 1. Задача FreeRTOS для асинхронного управления
 * 2. Очередь команд для межзадачного взаимодействия
 * 3. Поддержка повторяющихся сигналов
 * 4. Полный набор команд для всех событий системы
 * 5. Громкость при инициализации берётся из settings (EEPROM)
 * 6. ОТПРАВКА ЗВУКОВЫХ КОМАНД В ВЕБ-ИНТЕРФЕЙС ЧЕРЕЗ WEBSOCKET
 * ============================================================================
 */

#include "mp3_player.h"
#include "eeprom_settings.h"
#include "wifi_mqtt.h"  // ДЛЯ sendSoundCommand()

extern HardwareSerial dfplayerSerial;
extern DFRobotDFPlayerMini myDFPlayer;
extern QueueHandle_t mp3CommandQueue;
extern bool mp3PlayerReady;

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static bool mp3DebugOutput = true;
static bool repeatModeActive = false;
static uint32_t repeatIntervalMs = 10000;
static uint32_t lastRepeatTime = 0;
static uint16_t repeatTrackNumber = 5;  // По умолчанию 0005Avaria.mp3

#define MP3_CMD_ENABLE_REPEAT 10
#define MP3_CMD_DISABLE_REPEAT 11

// ============================================================================
// ОТПРАВКА ЗВУКОВОЙ КОМАНДЫ В ВЕБ-ИНТЕРФЕЙС
// ============================================================================
static void sendSoundToWeb(uint16_t trackNumber) {
  switch (trackNumber) {
    case 1:  // 0001.mp3 — стартовый / ожидание
      sendSoundCommand("zhdati");
      if (mp3DebugOutput) Serial.println("[MP3] Звук 'zhdati' отправлен в веб");
      break;
    case 2:  // 0002.mp3 — тревога / предупреждение
      sendSoundCommand("tormazi");
      if (mp3DebugOutput) Serial.println("[MP3] Звук 'tormazi' отправлен в веб");
      break;
    case 3:  // 0003.mp3 — переключение режима
      // Не отправляем звук в веб, только локально
      if (mp3DebugOutput) Serial.println("[MP3] Переключение режима (локально)");
      break;
    case 4:  // 0004.mp3 — ожидание (в жёлтом цикле)
      sendSoundCommand("zhdati");
      if (mp3DebugOutput) Serial.println("[MP3] Звук 'zhdati' отправлен в веб");
      break;
    case 5:  // 0005.mp3 — жёлтый цикл (авария)
      sendSoundCommand("yellow_start");
      if (mp3DebugOutput) Serial.println("[MP3] Звук 'yellow_start' отправлен в веб");
      break;
    case 6:  // 0006.mp3 — красный цикл
      sendSoundCommand("red_start");
      if (mp3DebugOutput) Serial.println("[MP3] Звук 'red_start' отправлен в веб");
      break;
    default:
      if (mp3DebugOutput) Serial.printf("[MP3] Трек #%d без веб-команды\n", trackNumber);
      break;
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

bool initMP3Player() {
  if (mp3DebugOutput) {
    Serial.println("[MP3] ========================================");
    Serial.println("[MP3] ИНИЦИАЛИЗАЦИЯ DFPLAYER MINI");
    Serial.println("[MP3] ========================================");
  }

  pinMode(DFPLAYER_BUSY_PIN, INPUT_PULLUP);
  delay(10);

  dfplayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);

  if (mp3DebugOutput) {
    Serial.printf("[MP3] UART2: RX=GPIO%d, TX=GPIO%d\n",
                  DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  }

  delay(500);

  if (!myDFPlayer.begin(dfplayerSerial, false, false)) {
    if (mp3DebugOutput) Serial.println("[MP3] ❌ DFPlayer не отвечает");
    mp3PlayerReady = false;
    return false;
  }

  myDFPlayer.setTimeOut(500);

  // Читаем громкость из настроек
  uint8_t startupVol = settings_get_mp3_volume();
  myDFPlayer.volume(startupVol);
  if (mp3DebugOutput) {
    Serial.printf("[MP3] Громкость при старте: %d (из EEPROM)\n", startupVol);
  }

  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  mp3PlayerReady = true;

  mp3CommandQueue = xQueueCreate(10, sizeof(Mp3Command_t));

  if (mp3CommandQueue == NULL) {
    if (mp3DebugOutput) Serial.println("[MP3] ❌ Не удалось создать очередь");
    mp3PlayerReady = false;
    return false;
  }

  repeatModeActive = false;

  if (mp3DebugOutput) {
    Serial.println("[MP3] ✅ Инициализация успешна");
    Serial.println("[MP3] ========================================\n");
  }

  return true;
}

// ============================================================================
// ЗАДАЧА MP3
// ============================================================================

void taskMP3(void* pvParameters) {
  if (mp3DebugOutput) Serial.println("[MP3] 🎵 Задача запущена");

  Mp3Command_t command;
  TickType_t lastWakeTime = xTaskGetTickCount();

  while (1) {
    uint32_t currentTime = pdTICKS_TO_MS(xTaskGetTickCount());

    // Проверка повтора
    if (repeatModeActive && mp3PlayerReady) {
      if (currentTime - lastRepeatTime >= repeatIntervalMs) {
        if (!isMP3Playing()) {
          myDFPlayer.play(repeatTrackNumber);
          // Отправляем звук в веб при повторе
          sendSoundToWeb(repeatTrackNumber);
        }
        lastRepeatTime = currentTime;
      }
    }

    // Обработка команд из очереди
    if (xQueueReceive(mp3CommandQueue, &command, 0) == pdTRUE) {

      switch (command.cmd) {
        case MP3_CMD_PLAY_TRACK:
          if (mp3DebugOutput) {
            Serial.printf("[MP3] ▶️ Трек #%d\n", command.param);
          }
          myDFPlayer.play(command.param);
          // ОТПРАВКА ЗВУКА В ВЕБ-ИНТЕРФЕЙС
          sendSoundToWeb(command.param);
          break;

        case MP3_CMD_SET_VOLUME:
          if (command.param > 30) command.param = 30;
          myDFPlayer.volume(command.param);
          if (mp3DebugOutput) {
            Serial.printf("[MP3] Громкость установлена: %d\n", command.param);
          }
          break;

        case MP3_CMD_STOP:
          if (mp3DebugOutput) Serial.println("[MP3] ⏹️ Стоп");
          repeatModeActive = false;
          myDFPlayer.stop();
          sendSoundCommand("stop_all");  // Остановить все звуки в вебе
          break;

        case MP3_CMD_PAUSE:
          myDFPlayer.pause();
          break;

        case MP3_CMD_RESUME:
          myDFPlayer.start();
          break;

        case MP3_CMD_ENABLE_REPEAT:
          if (command.param < 1) command.param = 10;
          repeatIntervalMs = command.param * 1000;
          repeatModeActive = true;
          repeatTrackNumber = command.param;
          lastRepeatTime = currentTime;
          if (mp3DebugOutput) {
            Serial.printf("[MP3] 🔁 Повтор включен, интервал %lu сек\n", command.param);
          }
          break;

        case MP3_CMD_DISABLE_REPEAT:
          repeatModeActive = false;
          if (mp3DebugOutput) Serial.println("[MP3] ⏹️ Повтор выключен");
          sendSoundCommand("stop_all");
          break;

        default:
          if (mp3DebugOutput) {
            Serial.printf("[MP3] ❓ Неизвестная команда: cmd=%d\n", command.cmd);
          }
          break;
      }

      vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(100));
  }
}

// ============================================================================
// ОТПРАВКА КОМАНД
// ============================================================================

bool sendMP3Command(Mp3Command_t command) {
  if (mp3CommandQueue == NULL) return false;

  if (xQueueSend(mp3CommandQueue, &command, 0) != pdTRUE) {
    if (mp3DebugOutput) Serial.println("[MP3] ⚠️ Очередь переполнена");
    return false;
  }

  return true;
}

bool isMP3Playing() {
  return digitalRead(DFPLAYER_BUSY_PIN) == LOW;
}