// C++ includes
#include <chrono>
#include <mutex>
#include <experimental/optional>
#include <queue>
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
#include "a2dp_cb.hpp"
#include "glue.hpp"
#include "gattc.hpp"

namespace std
{
	using namespace experimental;
}

using namespace std::literals;

namespace
{
	constexpr auto TAG = "GLUE";

	enum class state_t
	{
		IDLE,

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
	auto state = state_t::IDLE;


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


	std::queue<msg_t> messages;
	std::mutex message_mutex;

	std::size_t to_connect;
	int saved_conn_id;

	esp_bd_addr_t first_addr;
	esp_bd_addr_t second_addr;

	void handler()
	{
		const auto receive_msg = []() -> std::optional<msg_t>
        {
            std::lock_guard<std::mutex> l(message_mutex);

            if (!messages.empty())
            {
                msg_t msg = messages.front();
                messages.pop();
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

			switch (state)
			{
			case state_t::IDLE:
				break;

			/* IDLE TO BLE ALGORITHM */
			case state_t::IDLE_TO_BLE_0:
				if (msg == msg_t::IDLE_TO_BLE_START)
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Start Scanning");
					state = state_t::IDLE_TO_BLE_1;
					ESP_ERROR_CHECK(esp_ble_gap_start_scanning(5));
				}
				break;

			case state_t::IDLE_TO_BLE_1:
				if (msg == msg_t::SCAN_FINISHED)
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Kickstart connections");
					state = state_t::IDLE_TO_BLE_2;
					to_connect = 0;
					// Send an "empty" connected message to kickstart connecting process
					glue::notify_ble_connected();
				}
				break;

			case state_t::IDLE_TO_BLE_2:
				if (msg == msg_t::BLE_CONNECTED)
				{
					if (to_connect < bda.size())
					{
						ESP_LOGI(TAG, "IDLE_TO_BLE Open connection with %d", to_connect);
						state = state_t::IDLE_TO_BLE_3;
						ESP_ERROR_CHECK(esp_ble_gattc_open(
		            		interface,
		            		bda[to_connect],
		            		BLE_ADDR_TYPE_PUBLIC,
		            		true));
					}
					else
					{
						ESP_LOGI(TAG, "IDLE_TO_BLE Finished");
						state = state_t::IDLE;
					}
				}
				break;

			case state_t::IDLE_TO_BLE_3:
				if (msg == msg_t::BLE_OPENED)
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Send MTU negotation to %d", to_connect);
					state = state_t::IDLE_TO_BLE_4;
					ESP_ERROR_CHECK(esp_ble_gattc_send_mtu_req(interface, conn_id));
				}
				break;

			case state_t::IDLE_TO_BLE_4:
				if (msg == msg_t::MTU_CONFIGURED)
				{
					ESP_LOGI(TAG, "IDLE_TO_BLE Register for notifications with %d", to_connect);
					state = state_t::IDLE_TO_BLE_2;
					ESP_ERROR_CHECK(esp_ble_gattc_register_for_notify(
            			interface,
    					bda[to_connect],
    					0x2a));
					++to_connect;
				}
				break;


			/* BLE TO A2DP ALGORITHM */
			case state_t::BLE_TO_A2DP_0:
				if (msg == msg_t::BLE_TO_A2DP_START)
				{
					ESP_LOGI(TAG, "BLE_TO_A2DP Connect A2DP");
					state = state_t::BLE_TO_A2DP_1;
					ESP_ERROR_CHECK(esp_a2d_sink_connect(first_addr));
				}
				break;

			case state_t::BLE_TO_A2DP_1:
				if (msg == msg_t::A2DP_CONNECTED)
				{
					ESP_LOGI(TAG, "BLE_TO_A2DP Finished");
					state = state_t::IDLE;
				}
				break;


			/* A2DP TO A2DP ALGORITHM */
			case state_t::A2DP_TO_A2DP_0:
				if (msg == msg_t::A2DP_TO_A2DP_START)
				{
					ESP_LOGI(TAG, "A2DP_TO_A2DP Stop media stream");
					state = state_t::A2DP_TO_A2DP_1;
					ESP_ERROR_CHECK(esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP));
				}
				break;

			case state_t::A2DP_TO_A2DP_1:
				if (msg == msg_t::A2DP_MEDIA_STOPPED)
				{
					ESP_LOGI(TAG, "A2DP_TO_A2DP Disconnect A2DP");
					state = state_t::A2DP_TO_A2DP_2;
					ESP_ERROR_CHECK(esp_a2d_sink_disconnect(first_addr));
				}
				break;

			case state_t::A2DP_TO_A2DP_2:
				if (msg == msg_t::A2DP_DISCONNECTED)
				{
					ESP_LOGI(TAG, "A2DP_TO_A2DP Connect other A2DP");
					state = state_t::A2DP_TO_A2DP_3;
					ESP_ERROR_CHECK(esp_a2d_sink_connect(second_addr));
				}
				break;

			case state_t::A2DP_TO_A2DP_3:
				if (msg == msg_t::A2DP_CONNECTED)
				{
					ESP_LOGI(TAG, "A2DP_TO_A2DP Finished");
					state = state_t::IDLE;
				}
				break;


			/* A2DP TO BLE ALGORITM */
			case state_t::A2DP_TO_BLE_0:
				if (msg == msg_t::A2DP_TO_BLE_START)
				{
					ESP_LOGI(TAG, "A2DP_TO_BLE Stop media stream");
					state = state_t::A2DP_TO_BLE_1;
					ESP_ERROR_CHECK(esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP));
				}
				break;

			case state_t::A2DP_TO_BLE_1:
				if (msg == msg_t::A2DP_MEDIA_STOPPED)
				{
					ESP_LOGI(TAG, "A2DP_TO_BLE Disconnect A2DP");
					state = state_t::A2DP_TO_BLE_2;
					ESP_ERROR_CHECK(esp_a2d_sink_disconnect(first_addr));
				}
				break;

			case state_t::A2DP_TO_BLE_2:
				if (msg == msg_t::A2DP_DISCONNECTED)
				{
					// Disconnected A2DP
					ESP_LOGI(TAG, "A2DP_TO_BLE Finished");
					state = state_t::IDLE;
				}
				break;


			default:
				ESP_LOGE(TAG, "Unknown state %d", static_cast<int>(state));
			}
		}
	}

	void send_msg(std::queue<msg_t>& q, msg_t msg)
	{
		std::lock_guard<std::mutex> l(message_mutex);
		q.emplace(msg);
	}

	void notify_idle_to_ble_start()
	{
		send_msg(messages, msg_t::IDLE_TO_BLE_START);
	}

	void notify_ble_to_a2dp_start()
	{
		send_msg(messages, msg_t::BLE_TO_A2DP_START);
	}

	void notify_a2dp_to_a2dp_start()
	{
		send_msg(messages, msg_t::A2DP_TO_A2DP_START);
	}

	void notify_a2dp_to_ble_start()
	{
		send_msg(messages, msg_t::A2DP_TO_BLE_START);
	}
}

namespace glue
{
	void start_handler()
	{
		std::thread(handler).detach();
	}

	void idle_to_ble()
	{
		ESP_LOGI(TAG, "idle->ble");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(TAG, "Cannot start idle->ble switch when already switching modes");
			return;
		}

		state = state_t::IDLE_TO_BLE_0;
		notify_idle_to_ble_start();
	}

	void ble_to_a2dp(esp_bd_addr_t ble_addr)
	{
		ESP_LOGI(TAG, "ble->a2dp");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(TAG, "Cannot start ble->a2dp switch when already switching modes");
			return;
		}

		memcpy(first_addr, ble_addr, sizeof(esp_bd_addr_t));

		state = state_t::BLE_TO_A2DP_0;
		notify_ble_to_a2dp_start();
	}


	void a2dp_to_a2dp(esp_bd_addr_t old_addr, esp_bd_addr_t new_addr)
	{
		ESP_LOGI(TAG, "a2dp->a2dp");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(TAG, "Cannot start a2dp->a2dp switch when already switching modes");
			return;
		}

		memcpy(first_addr, old_addr, sizeof(esp_bd_addr_t));
		memcpy(second_addr, new_addr, sizeof(esp_bd_addr_t));

		state = state_t::A2DP_TO_A2DP_0;
		notify_a2dp_to_a2dp_start();
	}


	void a2dp_to_ble(esp_bd_addr_t addr)
	{
		ESP_LOGI(TAG, "a2dp->ble");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(TAG, "Cannot start a2dp->ble switch when already switching modes");
			return;
		}

		memcpy(first_addr, addr, sizeof(esp_bd_addr_t));

		state = state_t::A2DP_TO_BLE_0;
		notify_a2dp_to_ble_start();
	}

	void notify_scan_finished()
	{
		send_msg(messages, msg_t::SCAN_FINISHED);
	}

	void notify_ble_opened(int conn_id)
	{
		saved_conn_id = conn_id;
		send_msg(messages, msg_t::BLE_OPENED);
	}

	void notify_mtu_configured()
	{
		send_msg(messages, msg_t::MTU_CONFIGURED);
	}

	void notify_ble_connected()
	{
		send_msg(messages, msg_t::BLE_CONNECTED);
	}

	void notify_ble_disconnected()
	{
		send_msg(messages, msg_t::BLE_DISCONNECTED);
	}

	void notify_a2dp_connected()
	{
		send_msg(messages, msg_t::A2DP_CONNECTED);
	}

	void notify_a2dp_media_started()
	{
		send_msg(messages, msg_t::A2DP_MEDIA_STARTED);
	}

	void notify_a2dp_media_stopped()
	{
		send_msg(messages, msg_t::A2DP_MEDIA_STOPPED);
	}

	void notify_a2dp_disconnecting()
	{
		send_msg(messages, msg_t::A2DP_DISCONNECTING);
	}

	void notify_a2dp_disconnected()
	{
		send_msg(messages, msg_t::A2DP_DISCONNECTED);
	}
}
