// Matching include
#include "bluetooth_client.hpp"
// C includes
#include <cstring>
// ESP includes
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"

constexpr auto TAG = "A2DP_CB";

bluetooth_client::bluetooth_client()
	: m_servers()
	, m_sm()
	, m_peer(cend(m_servers))
    , m_interface(ESP_GATT_IF_NONE)
{
}

bluetooth_client& bluetooth_client::instance()
{
	static bluetooth_client client;
	return client;
}

void bluetooth_client::start()
{
	initialize();
	m_sm.start();
}

void bluetooth_client::initialize()
{
	static const auto a2dp_gap = [](
    	esp_bt_gap_cb_event_t event,
    	esp_bt_gap_cb_param_t *param)
    {
    	bluetooth_client::instance().a2dp_gap_callback(event, param);
    };

    static const auto a2dp = [](
    	esp_a2d_cb_event_t event,
    	esp_a2d_cb_param_t *a2d)
    {
    	bluetooth_client::instance().a2dp_callback(event, a2d);
    };

    static const auto a2dp_data = [](
    	const std::uint8_t *data,
    	std::uint32_t len)
    {
    	bluetooth_client::instance().a2dp_data_callback(data, len);
    };

    static const auto ble_gap = [](
    	esp_gap_ble_cb_event_t event,
    	esp_ble_gap_cb_param_t *param)
    {
    	bluetooth_client::instance().ble_gap_callback(event, param);
    };

    static const auto ble_gattc = [](
    	esp_gattc_cb_event_t event,
		esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param)
    {
    	bluetooth_client::instance().ble_gattc_callback(event, gattc_if, param);
    };

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_dev_set_device_name("CLIENT"));

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(a2dp_gap));
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp));
    ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(a2dp_data));
    ESP_ERROR_CHECK(esp_a2d_sink_init());
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE));

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(ble_gattc));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
}