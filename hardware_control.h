#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include "system_config.h"

void initHardware();
void findSensors();
void printAddress(uint8_t* addr);
void initFreeRTOSObjects();

#endif