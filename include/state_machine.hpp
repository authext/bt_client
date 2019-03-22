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

		A2DP_TO_BLE_0,
		A2DP_TO_BLE_1,
		A2DP_TO_BLE_2,
	};

	enum class msg_t
	{
		IDLE_TO_BLE_START,
		BLE_TO_A2DP_START,
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

	struct priority_msg_t
	{
		msg_t msg;
		int priority;
	};

	static bool is_priority_over(const priority_msg_t& l, const priority_msg_t& r);

	/* Constructors */
	state_machine();
	state_machine(const state_machine&) = delete;
	state_machine(state_machine&&) = default;

	/* Destructor */
	~state_machine() = default;

	/* Getters */
	const std::optional<bluetooth_address>& a2dp_address() const;

	/* Operators */
	state_machine& operator=(const state_machine&) = delete;
	state_machine& operator=(state_machine&&) = default;

	/* Methods */
	void start();
	// Switches from idle to (N BLE, 0 A2DP). Should be run only once, after scanning
	void idle_to_ble(std::uint16_t interface, std::vector<bluetooth_server_info> *servers);
	// Switches from (N BLE, 0 A2DP) to (N - 1 BLE, 1 A2DP)
	void ble_to_a2dp(bluetooth_address addr);
	// Switches from (N - 1 BLE, 1 A2DP) to (N BLE, 0 A2DP)
	void a2dp_to_ble();
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
	std::priority_queue<
		priority_msg_t,
		std::vector<priority_msg_t>,
		std::decay_t<decltype(is_priority_over)>> m_messages;
	std::mutex m_message_mutex;

	std::vector<bluetooth_server_info> *m_servers;
	state_t m_state;
	state_t m_saved_state;
	std::optional<bluetooth_address> m_a2dp_address;

	std::vector<bluetooth_server_info>::iterator m_to_connect;
	std::uint16_t m_interface;
	int m_saved_conn_id;

	/* Methods */
	void handler();
	void send_msg(msg_t msg, int priority);
	void notify_idle_to_ble_start();
	void notify_ble_to_a2dp_start();
	void notify_a2dp_to_ble_start();
};

#endif
