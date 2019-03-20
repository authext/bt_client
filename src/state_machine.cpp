// C++ includes
#include <algorithm>
#include <chrono>
#include <mutex>
#include <experimental/optional>
#include <queue>
#include <vector>
#include <thread>
// C includes
#include <cstring>
// Bluetooth includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
// Logging includes
#include "esp_log.h"
// My includes
#include "state_machine.hpp"


#include "bluetooth_client.hpp"

namespace std
{
	using namespace experimental;
}

using namespace std::literals;

namespace
{
	constexpr auto TAG = "STATE_MACHINE";

	std::vector<bluetooth_server_info>::iterator to_connect;
	std::uint16_t interface;
	int saved_conn_id;
}

const std::optional<bluetooth_address>& state_machine::a2dp_address() const
{
	return m_first_address;
}

void state_machine::start()
{
	std::thread([this]() { handler(); }).detach();
}

void state_machine::idle_to_ble(std::uint16_t iface, std::vector<bluetooth_server_info> *servers)
{
	ESP_LOGI(TAG, "idle->ble");

	if (m_state != state_t::IDLE)
	{
		ESP_LOGW(TAG, "Cannot start idle->ble switch when already switching modes");
		return;
	}

	m_servers = servers;
	interface = iface;
	m_state = state_t::IDLE_TO_BLE_0;
	notify_idle_to_ble_start();
}

void state_machine::ble_to_a2dp(bluetooth_address ble_addr)
{
	ESP_LOGI(TAG, "ble->a2dp");

	if (m_state != state_t::BLE)
	{
		ESP_LOGW(TAG, "Cannot start ble->a2dp switch when already switching modes");
		return;
	}

	m_first_address = ble_addr;
	m_second_address = {};

	m_state = state_t::BLE_TO_A2DP_0;
	notify_ble_to_a2dp_start();
}

void state_machine::a2dp_to_a2dp(bluetooth_address old_addr, bluetooth_address new_addr)
{
	ESP_LOGI(TAG, "a2dp->a2dp");

	if (m_state != state_t::A2DP)
	{
		ESP_LOGW(TAG, "Cannot start a2dp->a2dp switch when already switching modes");
		return;
	}

	m_first_address = old_addr;
	m_second_address = new_addr;

	m_state = state_t::A2DP_TO_A2DP_0;
	notify_a2dp_to_a2dp_start();
}

void state_machine::a2dp_to_ble(bluetooth_address addr)
{
	ESP_LOGI(TAG, "a2dp->ble");

	if (m_state != state_t::A2DP)
	{
		ESP_LOGW(TAG, "Cannot start a2dp->ble switch when already switching modes");
		return;
	}

	m_first_address = addr;
	m_second_address = {};

	m_state = state_t::A2DP_TO_BLE_0;
	notify_a2dp_to_ble_start();
}

void state_machine::notify_scan_finished()
{
	send_msg(msg_t::SCAN_FINISHED);
}

void state_machine::notify_ble_opened(int conn_id)
{
	saved_conn_id = conn_id;
	send_msg(msg_t::BLE_OPENED);
}

void state_machine::notify_mtu_configured()
{
	send_msg(msg_t::MTU_CONFIGURED);
}

void state_machine::notify_ble_connected()
{
	send_msg(msg_t::BLE_CONNECTED);
}

void state_machine::notify_ble_disconnected()
{
	send_msg(msg_t::BLE_DISCONNECTED);
}

void state_machine::notify_a2dp_connected()
{
	send_msg(msg_t::A2DP_CONNECTED);
}

void state_machine::notify_a2dp_media_started()
{
	send_msg(msg_t::A2DP_MEDIA_STARTED);
}

void state_machine::notify_a2dp_media_stopped()
{
	send_msg(msg_t::A2DP_MEDIA_STOPPED);
}

void state_machine::notify_a2dp_disconnecting()
{
	send_msg(msg_t::A2DP_DISCONNECTING);
}

void state_machine::notify_a2dp_disconnected()
{
	send_msg(msg_t::A2DP_DISCONNECTED);
}

void state_machine::handler()
{
	const auto receive_msg = [this]() -> std::optional<msg_t>
    {
        std::lock_guard<std::mutex> l(m_message_mutex);

        if (!m_messages.empty())
        {
            msg_t msg = m_messages.front();
            m_messages.pop();
            return msg;
        }
        else
        {
            return {};
        }
    };

	for (;;)
	{
		const auto opt_msg = receive_msg();
		if (!opt_msg)
		{
			std::this_thread::sleep_for(10ms);
			continue;
		}
		const auto msg = opt_msg.value();

		switch (m_state)
		{
		case state_t::IDLE:
		case state_t::BLE:
		case state_t::A2DP:
			break;

		/* IDLE TO BLE ALGORITHM */
		case state_t::IDLE_TO_BLE_0:
			if (msg == msg_t::IDLE_TO_BLE_START)
			{
				ESP_LOGI(TAG, "IDLE_TO_BLE Start Scanning");
				m_state = state_t::IDLE_TO_BLE_1;
				ESP_ERROR_CHECK(esp_ble_gap_start_scanning(5));
			}
			break;

		case state_t::IDLE_TO_BLE_1:
			if (msg == msg_t::SCAN_FINISHED)
			{
				if (m_servers->empty())
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE No servers found. Restarting");
					m_state = state_t::IDLE_TO_BLE_0;
					notify_idle_to_ble_start();
				}
				else
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Kickstart connections");
					m_state = state_t::IDLE_TO_BLE_2;
					to_connect = begin(*m_servers);
					// Send an "empty" connected message to kickstart connecting process
					notify_ble_connected();
				}
			}
			break;

		case state_t::IDLE_TO_BLE_2:
			if (msg == msg_t::BLE_CONNECTED)
			{
				if (to_connect != end(*m_servers))
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Open new connection with");
					m_state = state_t::IDLE_TO_BLE_3;
					ESP_ERROR_CHECK(esp_ble_gattc_open(
	            		interface,
	            		to_connect->address(),
	            		BLE_ADDR_TYPE_PUBLIC,
	            		true));
				}
				else
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Finished");
					m_state = state_t::BLE;
				}
			}
			break;

		case state_t::IDLE_TO_BLE_3:
			if (msg == msg_t::BLE_OPENED)
			{
				ESP_LOGI(TAG, "IDLE_TO_BLE Send MTU negotation to");
				m_state = state_t::IDLE_TO_BLE_4;
				ESP_ERROR_CHECK(esp_ble_gattc_send_mtu_req(interface, saved_conn_id));
			}
			break;

		case state_t::IDLE_TO_BLE_4:
			if (msg == msg_t::MTU_CONFIGURED)
			{
				ESP_LOGI(TAG, "IDLE_TO_BLE Register for notifications with");
				m_state = state_t::IDLE_TO_BLE_2;
				ESP_ERROR_CHECK(esp_ble_gattc_register_for_notify(
        			interface,
					to_connect->address(),
					0x2a));
				++to_connect;
			}
			break;


		/* BLE TO A2DP ALGORITHM */
		case state_t::BLE_TO_A2DP_0:
			if (msg == msg_t::BLE_TO_A2DP_START)
			{
				ESP_LOGI(TAG, "BLE_TO_A2DP Connect A2DP");
				m_state = state_t::BLE_TO_A2DP_1;
				ESP_ERROR_CHECK(esp_a2d_sink_connect(m_first_address.value()));
			}
			break;

		case state_t::BLE_TO_A2DP_1:
			if (msg == msg_t::A2DP_CONNECTED)
			{
				ESP_LOGI(TAG, "BLE_TO_A2DP Finished");
				m_state = state_t::A2DP;
			}
			else if (msg == msg_t::A2DP_DISCONNECTED)
			{
				ESP_LOGI(TAG, "BLE_TO_A2DP Failed");
				m_state = state_t::BLE;
			}
			break;


		/* A2DP TO A2DP ALGORITHM */
		case state_t::A2DP_TO_A2DP_0:
			if (msg == msg_t::A2DP_TO_A2DP_START)
			{
				ESP_LOGI(TAG, "A2DP_TO_A2DP Stop media stream");
				m_state = state_t::A2DP_TO_A2DP_1;
				ESP_ERROR_CHECK(esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP));
			}
			break;

		case state_t::A2DP_TO_A2DP_1:
			if (msg == msg_t::A2DP_MEDIA_STOPPED)
			{
				ESP_LOGI(TAG, "A2DP_TO_A2DP Disconnect A2DP");
				m_state = state_t::A2DP_TO_A2DP_2;
				ESP_ERROR_CHECK(esp_a2d_sink_disconnect(m_first_address.value()));
			}
			break;

		case state_t::A2DP_TO_A2DP_2:
			if (msg == msg_t::A2DP_DISCONNECTED)
			{
				ESP_LOGI(TAG, "A2DP_TO_A2DP Connect other A2DP");
				m_state = state_t::A2DP_TO_A2DP_3;
				ESP_ERROR_CHECK(esp_a2d_sink_connect(m_second_address.value()));
			}
			break;

		case state_t::A2DP_TO_A2DP_3:
			if (msg == msg_t::A2DP_CONNECTED)
			{
				ESP_LOGI(TAG, "A2DP_TO_A2DP Finished");
				m_state = state_t::A2DP;
				m_first_address = m_second_address;
				m_second_address = {};
			}
			break;


		/* A2DP TO BLE ALGORITM */
		case state_t::A2DP_TO_BLE_0:
			if (msg == msg_t::A2DP_TO_BLE_START)
			{
				ESP_LOGI(TAG, "A2DP_TO_BLE Stop media stream");
				m_state = state_t::A2DP_TO_BLE_1;
				ESP_ERROR_CHECK(esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP));
			}
			break;

		case state_t::A2DP_TO_BLE_1:
			if (msg == msg_t::A2DP_MEDIA_STOPPED)
			{
				ESP_LOGI(TAG, "A2DP_TO_BLE Disconnect A2DP");
				m_state = state_t::A2DP_TO_BLE_2;
				ESP_ERROR_CHECK(esp_a2d_sink_disconnect(m_first_address.value()));
			}
			break;

		case state_t::A2DP_TO_BLE_2:
			if (msg == msg_t::A2DP_DISCONNECTED)
			{
				// Disconnected A2DP
				ESP_LOGI(TAG, "A2DP_TO_BLE Finished");
				m_state = state_t::BLE;
				m_first_address = {};
			}
			break;


		default:
			ESP_LOGE(TAG, "Unknown m_state %d", static_cast<int>(m_state));
		}
	}
}

void state_machine::send_msg(msg_t msg)
{
	std::lock_guard<std::mutex> l(m_message_mutex);
	m_messages.emplace(msg);
}

void state_machine::notify_idle_to_ble_start()
{
	send_msg(msg_t::IDLE_TO_BLE_START);
}

void state_machine::notify_ble_to_a2dp_start()
{
	send_msg(msg_t::BLE_TO_A2DP_START);
}

void state_machine::notify_a2dp_to_a2dp_start()
{
	send_msg(msg_t::A2DP_TO_A2DP_START);
}

void state_machine::notify_a2dp_to_ble_start()
{
	send_msg(msg_t::A2DP_TO_BLE_START);
}