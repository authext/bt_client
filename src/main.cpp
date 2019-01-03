// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// ESP system includes
#include "esp_system.h"
// Logging includes
#include "esp_log.h"
// NVS includes
#include "nvs_flash.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
// My includes
#include "a2dp_cb.hpp"
#include "gattc.hpp"
#include "glue.hpp"

static const char *const CLIENT_TAG = "CLIENT";

extern "C" void app_main()
{
    esp_err_t ret;

    /* Initialize NVS. */
    if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s enable controller failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s enable controller failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s init bluetooth failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s enable bluetooth failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    a2dp_core_start();
    a2dp_core_dispatch(
        a2dp_cb_handle_stack_event,
    	A2D_CB_EVENT_STACK_UP,
    	NULL,
    	0);

    glue_start_handler();

    //register the  callback function to the gap module
    if ((ret = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s gap register failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    //register the callback function to the gattc module
    if((ret = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s gattc register failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_ble_gattc_app_register(0)) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"%s gattc app register failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_ble_gatt_set_local_mtu(500)) != ESP_OK)
    {
        ESP_LOGE(
        	CLIENT_TAG,
			"set local  MTU failed, error code = %x",
			ret);
    }
}
