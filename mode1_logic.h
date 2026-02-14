#ifndef MODE1_LOGIC_H
#define MODE1_LOGIC_H

#include "system_config.h"

void mode1_update_stabilization_timer(float guildTemp);
bool mode1_is_stabilization_counting();
String mode1_get_formatted_time();

#endif