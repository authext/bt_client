// C++ includes
#include <mutex>
#include <queue>
#include <thread>
// C includes
#include <cstring>
//
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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

namespace
{
	constexpr auto GLUE_TAG = "GLUE";

	enum state_t
	{
		IDLE,

		BLE_TO_A2DP_0,
		BLE_TO_A2DP_1,
		BLE_TO_A2DP_2,

		A2DP_TO_A2DP_0,
		A2DP_TO_A2DP_1,
		A2DP_TO_A2DP_2,
		A2DP_TO_A2DP_3,
		A2DP_TO_A2DP_4,

		A2DP_TO_BLE_0,
		A2DP_TO_BLE_1,
		A2DP_TO_BLE_2,
		A2DP_TO_BLE_3,
	};
	auto state = state_t::IDLE;


	enum msg_t
	{
		BLE_TO_A2DP_START,
		A2DP_TO_A2DP_START,
		A2DP_TO_BLE_START,

		BLE_CONNECTED,
		BLE_DISCONNECTED,

		A2DP_CONNECTED,
		A2DP_MEDIA_STOPPED,
		A2DP_DISCONNECTING,
		A2DP_DISCONNECTED,
	};


	xQueueHandle task_queue;

	esp_bd_addr_t first_addr;
	esp_bd_addr_t second_addr;

	void handler()
	{
		for (;;)
		{
			msg_t msg;
			if (xQueueReceive(task_queue, &msg, (portTickType)portMAX_DELAY) != pdTRUE)
				continue;

			switch (state)
			{
			case state_t::IDLE:
				break;


			/* BLE TO A2DP ALGORITHM */
			case state_t::BLE_TO_A2DP_0:
				if (msg == msg_t::BLE_TO_A2DP_START)
				{
					// Disconnecting BLE
					ESP_LOGI(GLUE_TAG, "BLE_TO_A2DP 0 -> 1");
					state = state_t::BLE_TO_A2DP_1;
					esp_ble_gap_disconnect(first_addr);
				}
				break;

			case state_t::BLE_TO_A2DP_1:
				if (msg == msg_t::BLE_DISCONNECTED)
				{
					// Disconnected BLE, connecting A2DP
					ESP_LOGI(GLUE_TAG, "BLE_TO_A2DP 1 -> 2");
					state = state_t::BLE_TO_A2DP_2;
					a2dp_cb::connect(first_addr);
				}
				break;

			case state_t::BLE_TO_A2DP_2:
				if (msg == msg_t::A2DP_CONNECTED)
				{
					// Connected A2DP
					ESP_LOGI(GLUE_TAG, "BLE_TO_A2DP 2 -> 3");
					state = state_t::IDLE;
				}
				break;


			/* A2DP TO A2DP ALGORITHM */
			case state_t::A2DP_TO_A2DP_0:
				if (msg == msg_t::A2DP_TO_A2DP_START)
				{
					// Stopping media
					ESP_LOGI(GLUE_TAG, "A2DP_TO_A2DP 0 -> 1");
					state = state_t::A2DP_TO_A2DP_1;
					esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
				}
				break;

			case state_t::A2DP_TO_A2DP_1:
				if (msg == msg_t::A2DP_MEDIA_STOPPED)
				{
					// Stopped media, disconnecting A2DP
					ESP_LOGI(GLUE_TAG, "A2DP_TO_A2DP 1 -> 2");
					state = state_t::A2DP_TO_A2DP_2;
					esp_a2d_sink_disconnect(first_addr);
				}
				break;

			case state_t::A2DP_TO_A2DP_2:
				if (msg == msg_t::A2DP_DISCONNECTED)
				{
					// Disconnected A2DP, connecting other A2DP
					ESP_LOGI(GLUE_TAG, "A2DP_TO_A2DP 2 -> 3");
					state = state_t::A2DP_TO_A2DP_3;
					a2dp_cb::connect(second_addr);
				}
				break;

			case state_t::A2DP_TO_A2DP_3:
				if (msg == msg_t::A2DP_CONNECTED)
				{
					// Connected A2DP, connecting other BLE
					ESP_LOGI(GLUE_TAG, "A2DP_TO_A2DP 3 -> 4");
					state = state_t::A2DP_TO_A2DP_4;
					esp_ble_gattc_open(
						interface,
						first_addr,
						BLE_ADDR_TYPE_PUBLIC,
						true);
				}
				break;

			case state_t::A2DP_TO_A2DP_4:
				if (msg == msg_t::BLE_CONNECTED)
				{
					// Connected BLE
					ESP_LOGI(GLUE_TAG, "A2DP_TO_A2DP 4 -> IDLE");
					state = state_t::IDLE;
				}
				break;


			/* A2DP TO BLE ALGORITM */
			case state_t::A2DP_TO_BLE_0:
				if (msg == msg_t::A2DP_TO_BLE_START)
				{
					// Stopping media
					ESP_LOGI(GLUE_TAG, "A2DP_TO_BLE 0 -> 1");
					state = state_t::A2DP_TO_BLE_1;
					esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
				}
				break;

			case state_t::A2DP_TO_BLE_1:
				if (msg == msg_t::A2DP_MEDIA_STOPPED)
				{
					// Stopped media, disconnecting A2DP
					ESP_LOGI(GLUE_TAG, "A2DP_TO_BLE 1 -> 2");
					state = state_t::A2DP_TO_BLE_2;
					esp_a2d_sink_disconnect(first_addr);
				}
				break;

			case state_t::A2DP_TO_BLE_2:
				if (msg == msg_t::A2DP_DISCONNECTED)
				{
					// Disconnected A2DP, connecting back BLE
					ESP_LOGI(GLUE_TAG, "A2DP_TO_BLE 2 -> 3");
					state = state_t::A2DP_TO_BLE_3;
					esp_ble_gattc_open(
						interface,
						first_addr,
						BLE_ADDR_TYPE_PUBLIC,
						true);
				}
				break;

			case state_t::A2DP_TO_BLE_3:
				if (msg == msg_t::BLE_CONNECTED)
				{
					// Connected BLE
					ESP_LOGI(GLUE_TAG, "A2DP_TO_BLE 3 -> IDLE");
					state = state_t::IDLE;
				}
				break;


			default:
				ESP_LOGE(GLUE_TAG, "Unknown state %d", state);
			}
		}
	}

	void notify_ble_to_a2dp_start()
	{
		static const auto msg = msg_t::BLE_TO_A2DP_START;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_a2dp_to_a2dp_start()
	{
		static const auto msg = msg_t::A2DP_TO_A2DP_START;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_a2dp_to_ble_start()
	{
		static const auto msg = msg_t::A2DP_TO_BLE_START;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}
}

namespace glue
{
	void start_handler()
	{
		task_queue = xQueueCreate(10, sizeof(msg_t));

		std::thread(handler).detach();
	}


	void stop_handler()
	{
		if (task_queue)
		{
			vQueueDelete(task_queue);
			task_queue = NULL;
		}
	}


	void ble_to_a2dp(esp_bd_addr_t ble_addr)
	{
		ESP_LOGI(GLUE_TAG, "ble->a2dp");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(GLUE_TAG, "Cannot start ble->a2dp switch when already switching modes");
			return;
		}

		memcpy(first_addr, ble_addr, sizeof(esp_bd_addr_t));

		state = state_t::BLE_TO_A2DP_0;
		notify_ble_to_a2dp_start();
	}


	void a2dp_to_a2dp(esp_bd_addr_t old_addr, esp_bd_addr_t new_addr)
	{
		ESP_LOGI(GLUE_TAG, "a2dp->a2dp");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(GLUE_TAG, "Cannot start a2dp->a2dp switch when already switching modes");
			return;
		}

		memcpy(first_addr, old_addr, sizeof(esp_bd_addr_t));
		memcpy(second_addr, new_addr, sizeof(esp_bd_addr_t));

		state = state_t::A2DP_TO_A2DP_0;
		notify_a2dp_to_a2dp_start();
	}


	void a2dp_to_ble(esp_bd_addr_t addr)
	{
		ESP_LOGI(GLUE_TAG, "a2dp->ble");

		if (state != state_t::IDLE)
		{
			ESP_LOGW(GLUE_TAG, "Cannot start a2dp->ble switch when already switching modes");
			return;
		}

		memcpy(first_addr, addr, sizeof(esp_bd_addr_t));

		state = state_t::A2DP_TO_BLE_0;
		notify_a2dp_to_ble_start();
	}

	void notify_ble_connected()
	{
		static const msg_t msg = msg_t::BLE_CONNECTED;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_ble_disconnected()
	{
		static const msg_t msg = msg_t::BLE_DISCONNECTED;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_a2dp_connected()
	{
		static const msg_t msg = msg_t::A2DP_CONNECTED;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_a2dp_media_stopped()
	{
		static const msg_t msg = msg_t::A2DP_MEDIA_STOPPED;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_a2dp_disconnecting()
	{
		static const msg_t msg = msg_t::A2DP_DISCONNECTING;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}

	void notify_a2dp_disconnected()
	{
		static const msg_t msg = msg_t::A2DP_DISCONNECTED;

		if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
		{
			ESP_LOGE(
				GLUE_TAG,
				"%s xQueue send failed",
				__func__);
		}
	}
}
