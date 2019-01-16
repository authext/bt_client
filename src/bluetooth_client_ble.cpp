// Matching include
#include "bluetooth_client.hpp"
// C++ includes
#include <algorithm>
// C includes
#include <cstring>
// ESP includes
#include "esp_gap_ble_api.h"
#include "esp_log.h"

namespace
{
	constexpr auto TAG = "CLIENT_BLE";
}

void bluetooth_client::ble_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    	if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT)
    	{
			esp_log_buffer_hex(
				TAG,
				param->scan_rst.bda,
				6);

			ESP_LOGI(
				TAG,
				"searched Adv Data Len %d, Scan Response Len %d",
				param->scan_rst.adv_data_len,
				param->scan_rst.scan_rsp_len);

			std::uint8_t adv_name_len;
			const auto *adv_name = (char *)esp_ble_resolve_adv_data(
				param->scan_rst.ble_adv,
				ESP_BLE_AD_TYPE_NAME_CMPL,
				&adv_name_len);

			if (adv_name == nullptr)
                break;

			constexpr auto DEVICE_NAME = "SERVER";
			if (strncmp(adv_name, DEVICE_NAME, strlen(DEVICE_NAME)) == 0)
			{
                esp_log_buffer_hex(
                    TAG,
                    param->scan_rst.bda,
                    sizeof(esp_bd_addr_t));
                const auto it = std::find_if(
                    cbegin(m_servers),
                    cend(m_servers),
                    [param](const auto& val)
                    {
                        return val.address() == bluetooth_address(param->scan_rst.bda);
                    });
                if (it == cend(m_servers))
                    m_servers.emplace_back(param->scan_rst.bda);
			}
			break;
		}

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        m_sm.notify_scan_finished();
    	break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(
        	TAG,
			"Connection updated\n"\
				"\tstatus = %d,\n"\
				"\tmin_int = %d,\n"\
				"\tmax_int = %d,\n"\
				"\tconn_int = %d,\n"\
				"\tlatency = %d,\n"\
				"\ttimeout = %d",
            param->update_conn_params.status,
            param->update_conn_params.min_int,
            param->update_conn_params.max_int,
            param->update_conn_params.conn_int,
            param->update_conn_params.latency,
            param->update_conn_params.timeout);
        break;

    default:
        break;
    }
}

void bluetooth_client::ble_gattc_callback(
	esp_gattc_cb_event_t event,
	esp_gatt_if_t gattc_if,
	esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT)
    {
    	if (param->reg.status == ESP_GATT_OK)
    	{
    		m_interface = gattc_if;
    	}
        else
    	{
    		ESP_LOGI(TAG,
    			"reg app failed, app_id %04x, status %d",
    			param->reg.app_id,
    			param->reg.status);
    		return;
    	}
    }

	const auto is_none = gattc_if == ESP_GATT_IF_NONE;
	const auto is_this = gattc_if == m_interface;

	if (!is_none && !is_this)
		return;

	switch (event)
    {
    case ESP_GATTC_REG_EVT:
    {
        ESP_LOGI(TAG, "REG_EVT");
        m_sm.idle_to_ble(m_interface, &m_servers);
        break;
    }

    case ESP_GATTC_CONNECT_EVT:
    {
        ESP_LOGI(
        	TAG,
			"ESP_GATTC_CONNECT_EVT conn_id %d, if %d",
			param->connect.conn_id,
			gattc_if);
        const auto conn_id = param->connect.conn_id;
        ESP_LOGI(
        	TAG,
			"REMOTE BDA:");
        esp_log_buffer_hex(
        	TAG,
			param->connect.remote_bda,
			sizeof(esp_bd_addr_t));
        auto it = std::find_if(
        	begin(m_servers),
        	end(m_servers),
        	[param](auto& val)
        	{
        		return val.address() == bluetooth_address(param->connect.remote_bda);
        	});
        if (it != end(m_servers))
        	it->conn_id(conn_id);
        m_sm.notify_ble_opened(conn_id);
        break;
    }

    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK)
        {
            ESP_LOGE(
            	TAG,
				"open failed, status %d",
				param->open.status);
            break;
        }
        ESP_LOGI(TAG, "open success");
        break;

    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK)
        {
            ESP_LOGE(
            	TAG,
				"config mtu failed, error status = %x",
				param->cfg_mtu.status);
        }
        ESP_LOGI(
        	TAG,
			"ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d",
			param->cfg_mtu.status,
			param->cfg_mtu.mtu,
			param->cfg_mtu.conn_id);
        m_sm.notify_mtu_configured();
        break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    	ESP_LOGI(TAG, "register for notify");
        m_sm.notify_ble_connected();
    	break;

    case ESP_GATTC_NOTIFY_EVT:
    {
    	ESP_LOGI(
    		TAG,
			"notified from boi %d: %d",
			param->notify.conn_id,
			param->notify.value[0]);

    	auto it = std::find_if(
    		begin(m_servers),
    		end(m_servers),
    		[param](const auto& val)
    		{
    			return param->notify.conn_id == val.conn_id();
    		});
    	if (it != end(m_servers))
    		it->activator(param->notify.value[0]);
    	handle_activator_notification();
    	break;
    }

    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(
        	TAG,
			"ESP_GATTC_DISCONNECT_EVT, reason = %d",
			param->disconnect.reason);
        m_sm.notify_ble_disconnected();
        break;

    default:
        break;
    }
}

void bluetooth_client::handle_activator_notification()
{
    const auto max_it = std::max_element(
        cbegin(m_servers),
        cend(m_servers),
        [](const auto& l, const auto& r)
        {
            return l.activator() < r.activator();
        });

    const auto current_it = std::find_if(
        cbegin(m_servers),
        cend(m_servers),
        [this](const auto& s)
        {
            return s.address() == m_sm.a2dp_address();
        });

    const auto addr = to_string(max_it->address());
    ESP_LOGI(TAG, "Max activator from %s: %d", addr.c_str(), max_it->activator());

    if (!m_sm.a2dp_address())
    {
        ESP_LOGI(TAG, "Want to switch from BLE to A2DP");
        m_sm.ble_to_a2dp(max_it->address());
    }
    else if (max_it->activator() > current_it->activator())
    {
        ESP_LOGI(TAG, "Want to swtich from A2DP to A2DP");
        m_sm.a2dp_to_a2dp(
            current_it->address(),
            max_it->address());
    }
    else if (current_it->activator() <= 2)
    {
        ESP_LOGI(TAG, "Want to switch from A2DP to BLE");
        m_sm.a2dp_to_ble(current_it->address()); 
    }
    else
    {
        ESP_LOGI(TAG, "Nothing to happen in regards to switching");
    }
}
