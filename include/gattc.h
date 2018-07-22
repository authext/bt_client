#ifndef GATT_H
#define GATT_H

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// ESP system includes
#include "esp_system.h"
// Logging includes
#include "esp_log.h"
// NVS includes
#include "nvs_flash.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

#define REMOTE_SERVICE_UUID        0x00FF
#define REMOTE_NOTIFY_CHAR_UUID    0xFF01

#define MAX_NUM_SERVERS 5
extern int len_servers;

extern uint8_t current_rms;
extern esp_bd_addr_t bda[MAX_NUM_SERVERS];
extern uint8_t rms[MAX_NUM_SERVERS];

typedef struct
{
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
} gattc_profile_inst;

extern gattc_profile_inst profile;

void esp_gap_cb(
	esp_gap_ble_cb_event_t event,
	esp_ble_gap_cb_param_t *param);
void esp_gattc_cb(
	esp_gattc_cb_event_t event,
	esp_gatt_if_t gattc_if,
	esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(
	esp_gattc_cb_event_t event,
	esp_gatt_if_t gattc_if,
	esp_ble_gattc_cb_param_t *param);

#endif
