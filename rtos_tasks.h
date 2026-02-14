#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

#include "system_config.h"
#include "mode1_logic.h"
#include "mode2_logic.h"
#include "serial_interface.h"

void taskMeasure(void* pv);
void taskDisplay(void* pv);
void taskSerial(void* pv);
void create_rtos_tasks();

#endif