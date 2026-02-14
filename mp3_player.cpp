/**
 * ============================================================================
 * ФАЙЛ: mp3_player.cpp
 * РЕАЛИЗАЦИЯ МОДУЛЯ УПРАВЛЕНИЯ DFPLAYER MINI (MP3-ПРОИГРЫВАТЕЛЕМ)
 * 
 * ВЕРСИЯ: 2.1 (С ИСПРАВЛЕННОЙ ЛОГИКОЙ ПОВТОРА)
 * ДАТА: [Текущая дата]
 * 
 * ОСОБЕННОСТИ:
 * 1. Задача FreeRTOS для асинхронного управления воспроизведением
 * 2. Очередь команд для межзадачного взаимодействия
 * 3. Поддержка повторяющихся сигналов для критических событий
 * 4. Режим повтора НЕ отключается при ручном запуске треков
 * 5. Расширенная система команд (включая управление таймерами)
 * ============================================================================
 */

#include "mp3_player.h"
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ============================================================================
// ВНЕШНИЕ ОБЪЯВЛЕНИЯ (определены в temperature_system.ino)
// ============================================================================
extern HardwareSerial dfplayerSerial;
extern DFRobotDFPlayerMini myDFPlayer;
extern QueueHandle_t mp3CommandQueue;
extern bool mp3PlayerReady;

// ============================================================================
// ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ И КОНСТАНТЫ
// ============================================================================

// ФЛАГИ ОТЛАДКИ И СОСТОЯНИЯ
static bool mp3DebugOutput = true;           // Вывод отладочной информации
static bool repeatModeActive = false;        // Флаг активного режима повтора
static uint32_t repeatIntervalMs = 10000;    // Интервал повтора (10 сек по умолчанию)
static uint32_t lastRepeatTime = 0;          // Время последнего повторения
static uint16_t repeatTrackNumber = 2;       // Трек для повтора (0002.mp3)

// СПЕЦИАЛЬНЫЕ КОМАНДЫ ДЛЯ УПРАВЛЕНИЯ ПОВТОРОМ
#define MP3_CMD_ENABLE_REPEAT  10   // Включить повтор: param = интервал в секундах
#define MP3_CMD_DISABLE_REPEAT 11   // Выключить повтор

// ============================================================================
// РЕАЛИЗАЦИЯ ПУБЛИЧНЫХ ФУНКЦИЙ
// ============================================================================

bool initMP3Player() {
    if (mp3DebugOutput) {
        Serial.println("[MP3] ========================================");
        Serial.println("[MP3] ИНИЦИАЛИЗАЦИЯ DFPLAYER MINI (v2.1)");
        Serial.println("[MP3] ========================================");
    }
    
    // 1. НАСТРОЙКА ПИНА BUSY КАК ВХОДА С ПОДТЯЖКОЙ
    pinMode(DFPLAYER_BUSY_PIN, INPUT_PULLUP);
    delay(10);
    
    // 2. ИНИЦИАЛИЗАЦИЯ АППАРАТНОГО UART2
    dfplayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
    
    if (mp3DebugOutput) {
        Serial.printf("[MP3] UART2: RX=GPIO%d, TX=GPIO%d\n", 
                     DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
        Serial.printf("[MP3] BUSY: GPIO%d (состояние: %s)\n",
                     DFPLAYER_BUSY_PIN, 
                     digitalRead(DFPLAYER_BUSY_PIN) ? "HIGH" : "LOW");
    }
    
    delay(500); // Критическая пауза для стабилизации связи
    
    // 3. ПОПЫТКА ИНИЦИАЛИЗАЦИИ ПЛЕЕРА
    if (mp3DebugOutput) Serial.println("[MP3] Подключение к DFPlayer...");
    
    if (!myDFPlayer.begin(dfplayerSerial, false, false)) {
        if (mp3DebugOutput) {
            Serial.println("[MP3] ❌ ОШИБКА: DFPlayer не отвечает!");
        }
        mp3PlayerReady = false;
        return false;
    }
    
    // 4. БАЗОВАЯ КОНФИГУРАЦИЯ ПЛЕЕРА
    myDFPlayer.setTimeOut(500);
    myDFPlayer.volume(15);
    myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
    
    if (mp3DebugOutput) {
        Serial.println("[MP3] ✅ DFPlayer успешно инициализирован");
        Serial.printf("[MP3] Текущая громкость: %d/30\n", myDFPlayer.readVolume());
    }
    
    mp3PlayerReady = true;
    
    // 5. СОЗДАНИЕ ОЧЕРЕДИ КОМАНД
    mp3CommandQueue = xQueueCreate(10, sizeof(Mp3Command_t));
    
    if (mp3CommandQueue == NULL) {
        if (mp3DebugOutput) Serial.println("[MP3] ❌ ОШИБКА: Не удалось создать очередь команд!");
        mp3PlayerReady = false;
        return false;
    }
    
    // 6. СБРОС СОСТОЯНИЙ ПОВТОРА
    repeatModeActive = false;
    repeatIntervalMs = 10000; // 10 секунд по умолчанию
    
    if (mp3DebugOutput) {
        Serial.println("[MP3] ✅ Очередь команд создана (10 слотов)");
        Serial.println("[MP3] 🔁 Модуль повтора сигналов готов");
        Serial.println("[MP3] ========================================\n");
    }
    
    return true;
}

void taskMP3(void* pvParameters) {
    if (mp3DebugOutput) Serial.println("[MP3] 🎵 Задача MP3 запущена (v2.1 с исправленным повтором)");
    
    Mp3Command_t command;
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (1) {
        uint32_t currentTime = pdTICKS_TO_MS(xTaskGetTickCount());
        
        // ============================================================
        // ПРОВЕРКА И ВЫПОЛНЕНИЕ ПОВТОРЯЮЩЕГОСЯ СИГНАЛА (если активен)
        // ============================================================
        if (repeatModeActive && mp3PlayerReady) {
            // Проверяем, пришло ли время для очередного повторения
            if (currentTime - lastRepeatTime >= repeatIntervalMs) {
                
                // Проверяем, не играет ли уже этот трек (чтобы не накладывать)
                if (!isMP3Playing()) {
                    if (mp3DebugOutput) {
                        Serial.printf("[MP3] 🔁 Автоповтор трека #%d (интервал: %lu сек)\n",
                                     repeatTrackNumber, repeatIntervalMs / 1000);
                    }
                    
                    // Проигрываем трек
                    myDFPlayer.play(repeatTrackNumber);
                } else {
                    // Если трек уже играет, просто обновляем таймер
                    if (mp3DebugOutput) Serial.println("[MP3] 🔁 Трек уже играет, пропускаю повтор");
                }
                
                // Обновляем время последнего повторения
                lastRepeatTime = currentTime;
            }
        }
        
        // ============================================================
        // ОБРАБОТКА КОМАНД ИЗ ОЧЕРЕДИ (НЕБЛОКИРУЮЩАЯ)
        // ============================================================
        if (xQueueReceive(mp3CommandQueue, &command, 0) == pdTRUE) {
            
            // Проверяем готовность плеера
            if (!mp3PlayerReady && command.cmd < MP3_CMD_ENABLE_REPEAT) {
                if (mp3DebugOutput) Serial.println("[MP3] ⚠️  Игнорирую команду - плеер не готов");
                continue;
            }
            
            // ОБРАБОТКА КОМАНД В ЗАВИСИМОСТИ ОТ ТИПА
            switch (command.cmd) {
                // ========== ОСНОВНЫЕ КОМАНДЫ DFPLAYER ==========
                case MP3_CMD_PLAY_TRACK:
                    if (mp3DebugOutput) {
                        Serial.printf("[MP3] ▶️  Воспроизведение трека #%d\n", command.param);
                    }
                    
                    // ИСПРАВЛЕНИЕ: НЕ отключаем режим повтора при ручном запуске трека
                    // Режим повтора остаётся активным, если был включен
                    // Это позволяет критическим сигналам повторяться даже при фоновой музыке
                    
                    myDFPlayer.play(command.param);
                    break;
                    
                case MP3_CMD_SET_VOLUME:
                    if (command.param > 30) command.param = 30;
                    if (mp3DebugOutput) {
                        Serial.printf("[MP3] 🔊 Установка громкости: %d/30\n", command.param);
                    }
                    myDFPlayer.volume(command.param);
                    break;
                    
                case MP3_CMD_STOP:
                    if (mp3DebugOutput) Serial.println("[MP3] ⏹️  Остановка воспроизведения");
                    
                    // ОСТАНОВКА ПОВТОРА при команде STOP
                    // Команда STOP явно говорит "прекратить всё воспроизведение"
                    if (repeatModeActive) {
                        repeatModeActive = false;
                        if (mp3DebugOutput) Serial.println("[MP3] ⏹️  Режим повтора отключен (команда STOP)");
                    }
                    
                    myDFPlayer.stop();
                    break;
                    
                case MP3_CMD_PAUSE:
                    if (mp3DebugOutput) Serial.println("[MP3] ⏸️  Пауза");
                    myDFPlayer.pause();
                    break;
                    
                case MP3_CMD_RESUME:
                    if (mp3DebugOutput) Serial.println("[MP3] ▶️  Продолжение воспроизведения");
                    myDFPlayer.start();
                    break;
                    
                case MP3_CMD_PLAY_FOLDER: {
                    uint8_t folder = (command.param >> 8) & 0xFF;
                    uint8_t track = command.param & 0xFF;
                    if (mp3DebugOutput) {
                        Serial.printf("[MP3] 📁 Воспроизведение: /%d/%03d.mp3\n", folder, track);
                    }
                    myDFPlayer.playFolder(folder, track);
                    break;
                }
                    
                case MP3_CMD_NEXT:
                    if (mp3DebugOutput) Serial.println("[MP3] ⏭️  Следующий трек");
                    myDFPlayer.next();
                    break;
                    
                case MP3_CMD_PREVIOUS:
                    if (mp3DebugOutput) Serial.println("[MP3] ⏮️  Предыдущий трек");
                    myDFPlayer.previous();
                    break;
                    
                // ========== СПЕЦИАЛЬНЫЕ КОМАНДЫ ДЛЯ ПОВТОРА ==========
                case MP3_CMD_ENABLE_REPEAT:
                    // ВКЛЮЧЕНИЕ РЕЖИМА ПОВТОРА СИГНАЛА
                    if (command.param < 1) command.param = 10; // Минимум 1 секунда
                    
                    repeatIntervalMs = command.param * 1000;   // Конвертируем секунды в мс
                    repeatModeActive = true;
                    lastRepeatTime = currentTime; // Сбрасываем таймер
                    
                    if (mp3DebugOutput) {
                        Serial.printf("[MP3] 🔁 Включен режим повтора: трек #%d, каждые %lu сек\n",
                                     repeatTrackNumber, command.param);
                    }
                    
                    // НЕМЕДЛЕННО ПРОИГРАТЬ ПЕРВЫЙ СИГНАЛ
                    if (mp3PlayerReady && !isMP3Playing()) {
                        myDFPlayer.play(repeatTrackNumber);
                        if (mp3DebugOutput) Serial.println("[MP3] ▶️  Первый сигнал повтора запущен");
                    }
                    break;
                    
                case MP3_CMD_DISABLE_REPEAT:
                    // ВЫКЛЮЧЕНИЕ РЕЖИМА ПОВТОРА
                    if (repeatModeActive) {
                        repeatModeActive = false;
                        if (mp3DebugOutput) {
                            Serial.println("[MP3] ⏹️  Режим повтора выключен");
                        }
                    }
                    break;
                    
                default:
                    if (mp3DebugOutput) {
                        Serial.printf("[MP3] ❓ Неизвестная команда: cmd=%d, param=%d\n", 
                                     command.cmd, command.param);
                    }
                    break;
            }
            
            // КОРОТКАЯ ПАУЗА ДЛЯ СТАБИЛЬНОСТИ СВЯЗИ
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // ============================================================
        // ОСНОВНАЯ ПАУЗА ЗАДАЧИ (СНИЖЕНИЕ НАГРУЗКИ НА ЦП)
        // ============================================================
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(100));
    }
}

bool sendMP3Command(Mp3Command_t command) {
    // ПРОВЕРКА СУЩЕСТВОВАНИЯ ОЧЕРЕДИ
    if (mp3CommandQueue == NULL) {
        if (mp3DebugOutput) Serial.println("[MP3] ❌ Ошибка: очередь команд не создана");
        return false;
    }
    
    // ПРОВЕРКА ГОТОВНОСТИ ПЛЕЕРА (для основных команд)
    if (command.cmd < MP3_CMD_ENABLE_REPEAT && 
        command.cmd != MP3_CMD_SET_VOLUME && 
        command.cmd != MP3_CMD_STOP) {
        if (!mp3PlayerReady) {
            if (mp3DebugOutput) Serial.println("[MP3] ⚠️  Ошибка: плеер не инициализирован");
            return false;
        }
    }
    
    // НЕБЛОКИРУЮЩАЯ ОТПРАВКА КОМАНДЫ В ОЧЕРЕДЬ
    if (xQueueSend(mp3CommandQueue, &command, 0) != pdTRUE) {
        if (mp3DebugOutput) Serial.println("[MP3] ⚠️  Ошибка: очередь команд переполнена");
        return false;
    }
    
    if (mp3DebugOutput) {
        Serial.printf("[MP3] 📤 Команда отправлена: cmd=%d, param=%d\n", 
                     command.cmd, command.param);
    }
    
    return true;
}

bool isMP3Playing() {
    // У DFPlayer Mini: BUSY = LOW - идет воспроизведение
    return (digitalRead(DFPLAYER_BUSY_PIN) == LOW);
}

bool playTrack(uint16_t trackNumber) {
    // ПРОВЕРКА: НЕ ИГРАЕТ ЛИ УЖЕ ЧТО-ТО
    if (isMP3Playing()) {
        if (mp3DebugOutput) Serial.println("[MP3] ⚠️  Пропускаю - уже идет воспроизведение");
        return false;
    }
    
    Mp3Command_t command;
    command.cmd = MP3_CMD_PLAY_TRACK;
    command.param = trackNumber;
    
    return sendMP3Command(command);
}

bool setVolume(uint8_t volume) {
    // ПРОВЕРКА ДОПУСТИМОГО ДИАПАЗОНА ГРОМКОСТИ
    if (volume > 30) {
        if (mp3DebugOutput) Serial.println("[MP3] ⚠️  Громкость ограничена до 30");
        volume = 30;
    }
    
    Mp3Command_t command;
    command.cmd = MP3_CMD_SET_VOLUME;
    command.param = volume;
    
    return sendMP3Command(command);
}