#ifndef A2DP_CB_H
#define A2DP_CB_H

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Unix includes
#include <unistd.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
// ESP System includes
#include "esp_system.h"
// Logging includes
#include "esp_log.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
// My includes
#include "a2dp_core.h"


typedef enum
{
    A2D_CB_EVENT_STACK_UP = 0,
} a2dp_cb_event_t;

/// handler for bluetooth stack enabled events
void a2dp_cb_handle_stack_event(uint16_t event, void *p_param);
esp_err_t a2dp_cb_connect(const esp_bd_addr_t addr);


#endif
