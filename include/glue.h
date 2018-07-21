#ifndef GLUE_H
#define GLUE_H

#include "esp_gattc_api.h"

void glue_start_handler();
void glue_stop_handler();

// Switches from (N BLE, 0 A2DP) to (N - 1 BLE, 1 A2DP)
void glue_ble_to_a2dp(esp_bd_addr_t ble_addr);
// Switches from (N - 1 BLE, 1 A2DP) to (N - 1 BLE, 1 A2DP)
void glue_a2dp_to_a2dp(esp_bd_addr_t old_addr, esp_bd_addr_t new_addr, esp_gatt_if_t ble_gatt_if);
// Switches from (N - 1 BLE, 1 A2DP) to (N BLE, 0 A2DP)
void glue_a2dp_to_ble(esp_bd_addr_t addr, esp_gatt_if_t ble_gatt_if);

void glue_notify_ble_connected();
void glue_notify_ble_disconnected();

void glue_notify_a2dp_connected();
void glue_notify_a2dp_disconnected();

#endif
