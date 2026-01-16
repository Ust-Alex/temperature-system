#include "measurement_core.h"

float filterValue(int sensorIdx, float newValue) {
  if (sensorIdx < 0 || sensorIdx >= 4) return newValue;

  Sensor_t* s = &sensors[sensorIdx];

  static bool bufferInitialized[4] = { false, false, false, false };

  if (!bufferInitialized[sensorIdx]) {
    for (int j = 0; j < 5; j++) {
      s->filterBuffer[j] = newValue;
    }
    s->filterSum = newValue * 5.0f;
    s->filterIndex = 0;
    bufferInitialized[sensorIdx] = true;
    return newValue;
  }

  s->filterSum -= s->filterBuffer[s->filterIndex];
  s->filterBuffer[s->filterIndex] = newValue;
  s->filterSum += newValue;
  s->filterIndex = (s->filterIndex + 1) % 5;

  return s->filterSum / 5.0f;
}

bool isValidTemperature(float temp) {
  return !(temp == TEMP_NO_DATA || temp == TEMP_SENSOR_LOST || temp == TEMP_CRITICAL_LOST || temp == -999.0f || temp < -50.0f || temp > 150.0f);
}

void safeUpdateSystemData(int idx, float temp, float delta) {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    sysData.temps[idx] = temp;
    sysData.deltas[idx] = delta;
    xSemaphoreGive(dataMutex);
  }
}

void safeReadSystemData(SystemData_t* data) {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    memcpy(data, &sysData, sizeof(SystemData_t));
    xSemaphoreGive(dataMutex);
  }
}

void attemptReconnect() {
  bool reconnectedAny = false;

  if (!sensors[3].found) {
    sensorsA.begin();
    int countA = sensorsA.getDeviceCount();

    if (countA > 0) {
      sensorsA.getAddress(sensors[3].addr, 0);
      sensors[3].found = true;
      sensorsA.setResolution(sensors[3].addr, RESOLUTION);
      sensors[3].lostTimer = 0;

      Serial.println("✅ Гильза восстановлена автоматически!");
      reconnectedAny = true;
      criticalError = false;
    }
  }

  sensorsB.begin();
  int countB = sensorsB.getDeviceCount();

  if (countB >= 3) {
    DeviceAddress foundAddrs[3];
    for (int i = 0; i < 3; i++) {
      sensorsB.getAddress(foundAddrs[i], i);
    }

    if (!sensors[0].found) {
      memcpy(sensors[0].addr, foundAddrs[2], 8);
      sensors[0].found = true;
      sensorsB.setResolution(sensors[0].addr, RESOLUTION);
      sensors[0].lostTimer = 0;
      Serial.println("✅ Датчик 100см восстановлен!");
      reconnectedAny = true;
    }

    if (!sensors[1].found) {
      memcpy(sensors[1].addr, foundAddrs[0], 8);
      sensors[1].found = true;
      sensorsB.setResolution(sensors[1].addr, RESOLUTION);
      sensors[1].lostTimer = 0;
      Serial.println("✅ Датчик 75см восстановлен!");
      reconnectedAny = true;
    }

    if (!sensors[2].found) {
      memcpy(sensors[2].addr, foundAddrs[1], 8);
      sensors[2].found = true;
      sensorsB.setResolution(sensors[2].addr, RESOLUTION);
      sensors[2].lostTimer = 0;
      Serial.println("✅ Датчик 50см восстановлен!");
      reconnectedAny = true;
    }
  }

  if (reconnectedAny) {
    Serial.println("✅ Автовосстановление завершено успешно");
    systemInitialized = sensors[3].found;
  }
}