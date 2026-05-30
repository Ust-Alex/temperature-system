/**
 * @file mode1_logic.cpp
 * @brief ЛОГИКА РЕЖИМА СТАБИЛИЗАЦИИ (MODE1)
 * @version 2.0 (ИЗМЕНЕНА: НЕ ОБНОВЛЯЕТ ТАЙМЕР ПРИ ОТСУТСТВИИ ГИЛЬЗЫ)
 * 
 * ОСНОВНЫЕ ИЗМЕНЕНИЯ:
 * - Добавлена проверка валидности температуры гильзы в начале функции
 * - Если температура невалидна (датчик отсутствует) — функция ничего не делает
 * - Это предотвращает сбои в расчётах и не засоряет логику
 */

#include "mode1_logic.h"

// ============================================================================
// ОБНОВЛЕНИЕ ТАЙМЕРА СТАБИЛИЗАЦИИ
// ============================================================================
void mode1_update_stabilization_timer(float guildTemp) {
  uint32_t currentMillis = millis();
  
  // ========================================================================
  // ИЗМЕНЕНИЕ: ПРОВЕРКА ВАЛИДНОСТИ ТЕМПЕРАТУРЫ ГИЛЬЗЫ
  // Если температура невалидна (датчик отсутствует, потерян, или вне диапазона)
  // то выходим из функции, не меняя состояние таймера.
  // Это безопасно: таймер просто не обновляется.
  // ========================================================================
  if (!isValidTemperature(guildTemp)) {
    return;  // ничего не делаем
  }
  
  if (sysData.mode == 0) {
    if (timeRefTemp == 0.0f) {
      // Первая валидная температура после запуска MODE1
      timeRefTemp = guildTemp;
      timeStartMs = currentMillis;
      timeIsCounting = false;
    } else if (fabs(guildTemp - timeRefTemp) <= 0.05f) {
      // Температура стабильна (отклонение не более 0.05°C)
      if (!timeIsCounting) {
        if ((currentMillis - timeStartMs) >= TIME_STABILIZATION) {
          // Прошло 10 секунд стабильности — запускаем отсчёт
          timeIsCounting = true;
          timeStartMs = currentMillis;
        }
      }
    } else {
      // Температура изменилась — сбрасываем таймер
      timeRefTemp = guildTemp;
      timeStartMs = currentMillis;
      timeIsCounting = false;
    }
  }
}

// ============================================================================
// ПОЛУЧЕНИЕ СТАТУСА ОТСЧЁТА СТАБИЛИЗАЦИИ
// ============================================================================
bool mode1_is_stabilization_counting() {
  return timeIsCounting;
}

// ============================================================================
// ФОРМИРОВАНИЕ СТРОКИ ВРЕМЕНИ ДЛЯ ДИСПЛЕЯ
// ============================================================================
String mode1_get_formatted_time() {
  if (timeIsCounting) {
    uint32_t elapsedMillis = millis() - timeStartMs;
    uint32_t elapsedMinutes = elapsedMillis / 60000UL;
    uint8_t hours = elapsedMinutes / 60;
    uint8_t minutes = elapsedMinutes % 60;

    char timeBuffer[6];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hours, minutes);
    return String(timeBuffer);
  }
  return "00:00";
}