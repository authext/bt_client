// C++ includes
#include <algorithm>
#include <vector>
// C includes
#include <cstring>
// My includes
#include "gattc.hpp"
#include "switching.hpp"
#include "glue.hpp"

std::uint16_t interface = ESP_GATT_IF_NONE;
std::uint16_t conn_id;
std::vector<bd_address> bda;
std::vector<std::uint8_t> rms;

namespace
{
    constexpr auto TAG = "GATT_CLIENT";

    void add_server(const bd_address addr)
    {
        const auto it = std::find(cbegin(bda), cend(bda), addr);
        if (it == cend(bda))
        {
            bda.emplace_back(addr);
            rms.emplace_back(0);
        }
    }

    void gattc_profile_event_handler(
    	esp_gattc_cb_event_t event,
    	esp_gatt_if_t gattc_if,
    	esp_ble_gattc_cb_param_t *param)
    {
        switch (event)
        {
        case ESP_GATTC_REG_EVT:
        {
            ESP_LOGI(TAG, "REG_EVT");
            glue::idle_to_ble();
            break;
        }

        case ESP_GATTC_CONNECT_EVT:
        {
            ESP_LOGI(
            	TAG,
    			"ESP_GATTC_CONNECT_EVT conn_id %d, if %d",
    			param->connect.conn_id,
    			gattc_if);
            conn_id = param->connect.conn_id;
            ESP_LOGI(
            	TAG,
    			"REMOTE BDA:");
            esp_log_buffer_hex(
            	TAG,
    			param->connect.remote_bda,
    			sizeof(esp_bd_addr_t));
            glue::notify_ble_opened(conn_id);
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
            glue::notify_mtu_configured();
            break;

        case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        	ESP_LOGI(TAG, "register for notify");
            glue::notify_ble_connected();
        	break;

        case ESP_GATTC_NOTIFY_EVT:
        	ESP_LOGI(
        		TAG,
    			"notified from boi %d: %d",
    			param->notify.conn_id,
    			param->notify.value[0]);
        	rms[param->notify.conn_id] = param->notify.value[0];
        	switching::handle_rms_notification();
        	break;

        case ESP_GATTC_DISCONNECT_EVT:
            ESP_LOGI(
            	TAG,
    			"ESP_GATTC_DISCONNECT_EVT, reason = %d",
    			param->disconnect.reason);
            glue::notify_ble_disconnected();
            break;

        default:
            break;
        }
    }
}

namespace gattc
{
    void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
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

    			if (adv_name != nullptr)
    			{
    				constexpr auto DEVICE_NAME = "SERVER";
    				if (strncmp(adv_name, DEVICE_NAME, strlen(DEVICE_NAME)) == 0)
    				{
    					ESP_LOGI(TAG, "Maybe adding a server");
                        esp_log_buffer_hex(
                            TAG,
                            param->scan_rst.bda,
                            sizeof(esp_bd_addr_t));
    					add_server(param->scan_rst.bda);
    				}
    			}
    			break;
    		}

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            glue::notify_scan_finished();
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

    void esp_gattc_cb(
    	esp_gattc_cb_event_t event,
    	esp_gatt_if_t gattc_if,
    	esp_ble_gattc_cb_param_t *param)
    {
        /* If event is register event, store the gattc_if for each profile */
        if (event == ESP_GATTC_REG_EVT)
        {
        	if (param->reg.status == ESP_GATT_OK)
        	{
        		interface = gattc_if;
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
    	const auto is_this = gattc_if == interface;

    	/* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
    	if (is_none || is_this)
    			gattc_profile_event_handler(event, gattc_if, param);
    }
}
