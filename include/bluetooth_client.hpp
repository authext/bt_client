#ifndef BLUETOOTH_CLIENT_HPP
#define BLUETOOTH_CLIENT_HPP

// C++ includes
#include <experimental/optional>
#include <functional>
#include <vector>
#include <tuple>
// C includes
#include <cstdint>
// My includes
#include "bluetooth_server.hpp"
#include "state_machine.hpp"
// ESP includes
#include "esp_a2dp_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gap_bt_api.h"

namespace std
{
	using namespace experimental;
}

class bluetooth_client
{
public:
	/* Constructors */
	bluetooth_client();
	bluetooth_client(const bluetooth_client&) = delete;
	bluetooth_client(bluetooth_client&&) = delete;

	/* Destructor */
	~bluetooth_client() = default;

	/* Operators */
	bluetooth_client& operator=(const bluetooth_client&) = delete;
	bluetooth_client& operator=(bluetooth_client&&) = delete;

	/* Methods */
	void start();

	/* Static getters */
	static bluetooth_client& instance();

private:
	/* Members */
	std::vector<bluetooth_server> m_servers;
	state_machine m_sm;
	decltype(m_servers)::const_iterator m_peer;
	std::uint16_t m_interface;

	/* Methods */
	void initialize();

	void a2dp_gap_callback(
		esp_bt_gap_cb_event_t event,
    	esp_bt_gap_cb_param_t *param);
	void a2dp_callback(
		esp_a2d_cb_event_t event,
		esp_a2d_cb_param_t *a2d);
	void a2dp_data_callback(
		const std::uint8_t *data,
		std::uint32_t len);

	void ble_gap_callback(
		esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param);
	void ble_gattc_callback(
		esp_gattc_cb_event_t event,
		esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param);

	int current_a2dp_idx = -1;
	void handle_rms_notification();
};

#endif
