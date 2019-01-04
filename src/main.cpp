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
#include "a2dp_core.hpp"
#include "a2dp_cb.hpp"
#include "gattc.hpp"
#include "glue.hpp"

namespace
{
    constexpr auto TAG = "CLIENT";
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    a2dp_core::start();
    a2dp_core::dispatch(
        a2dp_cb::init_stack,
    	0,
    	nullptr,
    	0);

    glue_start_handler();

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gattc::esp_gap_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(gattc::esp_gattc_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
}
