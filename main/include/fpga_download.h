#pragma once

#include <freertos/FreeRTOS.h>

#include "ice40.h"

void fpga_download(xQueueHandle button_queue, ICE40* ice40);

typedef bool (*fpga_host_fn_t)(xQueueHandle buttonQueue, ICE40* ice40, bool enable_uart, const char* prefix);
bool fpga_host(xQueueHandle button_queue, ICE40* ice40, bool enable_uart, const char* prefix);
bool my_fpga_host(xQueueHandle button_queue, ICE40* ice40, bool enable_uart, const char* prefix);
