#ifndef SERIAL_INTERFACE_H
#define SERIAL_INTERFACE_H

#include "system_config.h"

void serial_process_command(String command);
void serial_handle_input();
void serial_print_status();
void serial_print_help();

#endif