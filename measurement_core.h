#ifndef MEASUREMENT_CORE_H
#define MEASUREMENT_CORE_H

#include "system_config.h"

float filterValue(int sensorIdx, float newValue);
bool isValidTemperature(float temp);
void safeUpdateSystemData(int idx, float temp, float delta);
void safeReadSystemData(SystemData_t* data);
void attemptReconnect();

#endif