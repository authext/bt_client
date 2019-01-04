#ifndef GATT_HPP
#define GATT_HPP

// C includes
#include <cstdint>
// Logging includes
#include "esp_log.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

#define MAX_NUM_SERVERS 5
extern int len_servers;

extern std::uint16_t interface;
extern std::uint16_t conn_id;
extern std::uint8_t current_rms;
extern esp_bd_addr_t bda[MAX_NUM_SERVERS];
extern std::uint8_t rms[MAX_NUM_SERVERS];


namespace gattc
{
	void esp_gap_cb(
		esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param);
	void esp_gattc_cb(
		esp_gattc_cb_event_t event,
		esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param);
}

#endif
