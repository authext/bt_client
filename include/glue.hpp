#ifndef GLUE_HPP
#define GLUE_HPP

#include "esp_gattc_api.h"

namespace glue
{
	void start_handler();

	// Switches from idle to (N BLE, 0 A2DP). Should be run only once, after scanning
	void idle_to_ble();
	// Switches from (N BLE, 0 A2DP) to (N - 1 BLE, 1 A2DP)
	void ble_to_a2dp(esp_bd_addr_t ble_addr);
	// Switches from (N - 1 BLE, 1 A2DP) to (N - 1 BLE, 1 A2DP)
	void a2dp_to_a2dp(esp_bd_addr_t old_addr, esp_bd_addr_t new_addr);
	// Switches from (N - 1 BLE, 1 A2DP) to (N BLE, 0 A2DP)
	void a2dp_to_ble(esp_bd_addr_t addr);

	void notify_scan_finished();
	void notify_ble_opened(int conn_id);
	void notify_mtu_configured();

	void notify_ble_connected();
	void notify_ble_disconnected();

	void notify_a2dp_connected();
	void notify_a2dp_media_stopped();
	void notify_a2dp_disconnecting();
	void notify_a2dp_disconnected();
}

#endif
