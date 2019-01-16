#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

// C++ includes
#include <experimental/optional>
#include <mutex>
#include <queue>
#include <vector>
// ESP includes
#include "esp_gattc_api.h"
// My includes
#include "bluetooth_server_info.hpp"

namespace std
{
	using namespace experimental;
}

class state_machine
{
public:
	/* Inner types */
	enum class state_t
	{
		IDLE,
		BLE,
		A2DP,

		IDLE_TO_BLE_0,
		IDLE_TO_BLE_1,
		IDLE_TO_BLE_2,
		IDLE_TO_BLE_3,
		IDLE_TO_BLE_4,

		BLE_TO_A2DP_0,
		BLE_TO_A2DP_1,

		A2DP_TO_A2DP_0,
		A2DP_TO_A2DP_1,
		A2DP_TO_A2DP_2,
		A2DP_TO_A2DP_3,

		A2DP_TO_BLE_0,
		A2DP_TO_BLE_1,
		A2DP_TO_BLE_2,
	};

	enum class msg_t
	{
		IDLE_TO_BLE_START,
		BLE_TO_A2DP_START,
		A2DP_TO_A2DP_START,
		A2DP_TO_BLE_START,

		SCAN_FINISHED,
		BLE_OPENED,
		MTU_CONFIGURED,

		BLE_CONNECTED,
		BLE_DISCONNECTED,

		A2DP_CONNECTED,
		A2DP_MEDIA_STOPPED,
		A2DP_MEDIA_STARTED,
		A2DP_DISCONNECTING,
		A2DP_DISCONNECTED,
	};

	/* Constructors */
	state_machine() = default;
	state_machine(const state_machine&) = delete;
	state_machine(state_machine&&) = default;

	/* Destructor */
	~state_machine() = default;

	/* Operators */
	state_machine& operator=(const state_machine&) = delete;
	state_machine& operator=(state_machine&&) = default;

	/* Methods */
	void start();
	// Switches from idle to (N BLE, 0 A2DP). Should be run only once, after scanning
	void idle_to_ble(std::uint16_t interface, std::vector<bluetooth_server_info> *servers);
	// Switches from (N BLE, 0 A2DP) to (N - 1 BLE, 1 A2DP)
	void ble_to_a2dp(bluetooth_address ble_addr);
	// Switches from (N - 1 BLE, 1 A2DP) to (N - 1 BLE, 1 A2DP)
	void a2dp_to_a2dp(bluetooth_address old_addr, bluetooth_address new_addr);
	// Switches from (N - 1 BLE, 1 A2DP) to (N BLE, 0 A2DP)
	void a2dp_to_ble(bluetooth_address addr);
	void notify_scan_finished();
	void notify_ble_opened(int conn_id);
	void notify_mtu_configured();

	void notify_ble_connected();
	void notify_ble_disconnected();

	void notify_a2dp_connected();
	void notify_a2dp_media_started();
	void notify_a2dp_media_stopped();
	void notify_a2dp_disconnecting();
	void notify_a2dp_disconnected();

private:
	/* Members */
	std::queue<msg_t> m_messages;
	std::mutex m_message_mutex;

	std::vector<bluetooth_server_info> *m_servers;
	state_t m_state = state_t::IDLE;
	std::optional<bluetooth_address> m_first_address;
	std::optional<bluetooth_address> m_second_address;

	/* Methods */
	void handler();
	void send_msg(msg_t msg);
	void notify_idle_to_ble_start();
	void notify_ble_to_a2dp_start();
	void notify_a2dp_to_a2dp_start();
	void notify_a2dp_to_ble_start();
};

#endif
