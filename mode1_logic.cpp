/**
 * @file mode1_logic.cpp
 * @brief ЛОГИКА РЕЖИМА СТАБИЛИЗАЦИИ (MODE1)
 * @version 2.0
 */

#include "mode1_logic.h"

void mode1_update_stabilization_timer(float guildTemp) {
  uint32_t currentMillis = millis();
  
  if (!isValidTemperature(guildTemp)) {
    return;
  }
  
  if (sysData.mode == 0) {
    if (timeRefTemp == 0.0f) {
      timeRefTemp = guildTemp;
      timeStartMs = currentMillis;
      timeIsCounting = false;
    } else if (fabs(guildTemp - timeRefTemp) <= 0.05f) {
      if (!timeIsCounting) {
        if ((currentMillis - timeStartMs) >= TIME_STABILIZATION) {
          timeIsCounting = true;
          timeStartMs = currentMillis;
        }
      }
    } else {
      timeRefTemp = guildTemp;
      timeStartMs = currentMillis;
      timeIsCounting = false;
    }
  }
}

bool mode1_is_stabilization_counting() {
  return timeIsCounting;
}

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