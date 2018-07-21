// C includes
#include <stdbool.h>
#include <string.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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
#include "glue.h"
#include "tags.h"


typedef enum
{
	GLUE_STATE_IDLE,

	GLUE_STATE_BLE_TO_A2DP_0,
	GLUE_STATE_BLE_TO_A2DP_1,

	GLUE_STATE_A2DP_TO_A2DP_0,
	GLUE_STATE_A2DP_TO_A2DP_1,
	GLUE_STATE_A2DP_TO_A2DP_2,

	GLUE_STATE_A2DP_TO_BLE_0,
	GLUE_STATE_A2DP_TO_BLE_1,
} glue_state_t;
static glue_state_t glue_state = GLUE_STATE_IDLE;


typedef enum
{
	GLUE_MSG_BLE_CONNECTED,
	GLUE_MSG_BLE_DISCONNECTED,

	GLUE_MSG_A2DP_CONNECTED,
	GLUE_MSG_A2DP_DISCONNECTED,
} glue_msg_t;


static xQueueHandle task_queue;
static xTaskHandle task_handle;

static esp_bd_addr_t first_addr;
static esp_bd_addr_t second_addr;
static esp_gatt_if_t gatt_if;


static void glue_handler(void *_)
{
	for (;;)
	{
		glue_msg_t msg;
		if (xQueueReceive(task_queue, &msg, (portTickType)portMAX_DELAY) != pdTRUE)
			continue;

		switch (glue_state)
		{
		case GLUE_STATE_IDLE:
			break;


		case GLUE_STATE_BLE_TO_A2DP_0:
			if (msg == GLUE_MSG_BLE_DISCONNECTED)
			{
				// Disconnected BLE, connecting A2DP
				glue_state = GLUE_STATE_BLE_TO_A2DP_1;
				esp_a2d_sink_connect(first_addr);
			}
			break;

		case GLUE_STATE_BLE_TO_A2DP_1:
			if (msg == GLUE_MSG_A2DP_CONNECTED)
			{
				// Connected A2DP
				glue_state = GLUE_STATE_IDLE;
			}
			break;


		case GLUE_STATE_A2DP_TO_A2DP_0:
			if (msg == GLUE_MSG_A2DP_DISCONNECTED)
			{
				// Disconnected A2DP, connecting other A2DP
				glue_state = GLUE_STATE_A2DP_TO_A2DP_1;
				esp_a2d_sink_connect(second_addr);
			}
			break;

		case GLUE_STATE_A2DP_TO_A2DP_1:
			if (msg == GLUE_MSG_A2DP_CONNECTED)
			{
				// Connected A2DP, connecting other BLE
				glue_state = GLUE_STATE_A2DP_TO_A2DP_2;
				esp_ble_gattc_open(
					gatt_if,
					first_addr,
					BLE_ADDR_TYPE_PUBLIC,
					true);
			}
			break;

		case GLUE_STATE_A2DP_TO_A2DP_2:
			if (msg == GLUE_MSG_BLE_CONNECTED)
			{
				// Connected BLE
				glue_state = GLUE_STATE_IDLE;
			}
			break;


		case GLUE_STATE_A2DP_TO_BLE_0:
			if (msg == GLUE_MSG_A2DP_DISCONNECTED)
			{
				// Disconnected A2DP, connecting back BLE
				glue_state = GLUE_STATE_A2DP_TO_BLE_1;
				esp_ble_gattc_open(
					gatt_if,
					first_addr,
					BLE_ADDR_TYPE_PUBLIC,
					true);
			}
			break;

		case GLUE_STATE_A2DP_TO_BLE_1:
			if (msg == GLUE_MSG_BLE_CONNECTED)
			{
				// Connected BLE
				glue_state = GLUE_STATE_IDLE;
			}
			break;

		default:
			ESP_LOGE(GLUE_TAG, "Unknown state %d", glue_state);
		}
	}
}

void glue_start_handler()
{
	task_queue = xQueueCreate(10, sizeof(glue_msg_t));

	xTaskCreate(
		glue_handler,
		"glue_handler",
		4096,
		NULL,
		configMAX_PRIORITIES - 1,
		&task_handle);
}


void glue_stop_handler()
{
	if (task_handle)
	{
		vTaskDelete(task_handle);
		task_handle = NULL;
	}

	if (task_queue)
	{
		vQueueDelete(task_queue);
		task_queue = NULL;
	}
}


void glue_ble_to_a2dp(esp_bd_addr_t ble_addr)
{
	if (glue_state != GLUE_STATE_IDLE)
		ESP_LOGW(GLUE_TAG, "Cannot start ble->a2dp switch when already switching modes");

	memcpy(first_addr, ble_addr, sizeof(esp_bd_addr_t));

	glue_state = GLUE_STATE_BLE_TO_A2DP_0;
	esp_ble_gap_disconnect(first_addr);
}


void glue_a2dp_to_a2dp(esp_bd_addr_t old_addr, esp_bd_addr_t new_addr, esp_gatt_if_t ble_gatt_if)
{
	if (glue_state != GLUE_STATE_IDLE)
		ESP_LOGW(GLUE_TAG, "Cannot start a2dp->a2dp switch when already switching modes");

	memcpy(first_addr, old_addr, sizeof(esp_bd_addr_t));
	memcpy(second_addr, new_addr, sizeof(esp_bd_addr_t));
	gatt_if = ble_gatt_if;

	glue_state = GLUE_STATE_A2DP_TO_A2DP_0;
	esp_a2d_sink_disconnect(first_addr);
}


void glue_a2dp_to_ble(esp_bd_addr_t addr, esp_gatt_if_t ble_gatt_if)
{
	if (glue_state != GLUE_STATE_IDLE)
		ESP_LOGW(GLUE_TAG, "Cannot start a2dp->ble switch when already switching modes");

	memcpy(first_addr, addr, sizeof(esp_bd_addr_t));
	gatt_if = ble_gatt_if;

	// todo start switching
	glue_state = GLUE_STATE_A2DP_TO_BLE_0;
	esp_a2d_sink_disconnect(first_addr);
}


void glue_notify_ble_connected()
{
	static const glue_msg_t msg = GLUE_MSG_BLE_CONNECTED;

	if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
	{
		ESP_LOGE(
			GLUE_TAG,
			"%s xQueue send failed",
			__func__);
	}
}

void glue_notify_ble_disconnected()
{
	static const glue_msg_t msg = GLUE_MSG_BLE_DISCONNECTED;

	if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
	{
		ESP_LOGE(
			GLUE_TAG,
			"%s xQueue send failed",
			__func__);
	}
}

void glue_notify_a2dp_connected()
{
	static const glue_msg_t msg = GLUE_MSG_A2DP_CONNECTED;

	if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
	{
		ESP_LOGE(
			GLUE_TAG,
			"%s xQueue send failed",
			__func__);
	}
}

void glue_notify_a2dp_disconnected()
{
	static const glue_msg_t msg = GLUE_MSG_A2DP_DISCONNECTED;

	if (xQueueSend(task_queue, &msg, 10 / portTICK_RATE_MS) != pdTRUE)
	{
		ESP_LOGE(
			GLUE_TAG,
			"%s xQueue send failed",
			__func__);
	}
}
