#include "gatt.h"
#include "tag.h"

#define LEN_OF(x) (sizeof(x) / sizeof(x[0]))

static esp_bt_uuid_t remote_filter_service_uuid =
{
    .len = ESP_UUID_LEN_16,
    .uuid =
    {
    	.uuid16 = REMOTE_SERVICE_UUID,
    },
};

static esp_bt_uuid_t remote_filter_char_uuid =
{
    .len = ESP_UUID_LEN_16,
    .uuid =
    {
    	.uuid16 = REMOTE_NOTIFY_CHAR_UUID,
    },
};

static esp_bd_addr_t bda[] =
{
	{ 0x30, 0xae, 0xa4, 0x5d, 0xc6, 0x76 },
	{ 0x30, 0xae, 0xa4, 0x74, 0x1c, 0xf2 },
};

static uint8_t rms[] =
{
	0,
	0
};

int current_a2dp_idx = -1;
static uint16_t rms_handle;

static bool connect    = false;
static bool get_server = false;

void switch_to_a2dp(size_t idx)
{
	esp_ble_gap_disconnect(bda[idx]);

	current_a2dp_idx = idx;
}

void handle_rms_notification()
{
	uint8_t max_rms = 0;
	size_t max_idx = 0;

	for (size_t i = 0; i < LEN_OF(rms); i++)
	{
		if (rms[i] > max_rms)
		{
			max_rms = rms[i];
			max_idx = i;
		}
	}

	ESP_LOGI(TAG, "Max rms from %d: %d", max_idx, max_rms);

	if (max_rms > rms[current_a2dp_idx])
	{
		ESP_LOGI(
			TAG,
			"Better than current, switching from %d to %d",
			current_a2dp_idx,
			max_idx);
		switch_to_a2dp(max_idx);
	}
}

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
gattc_profile_inst profile_tab[PROFILE_NUM] =
{
    [PROFILE_A_APP_ID] =
    {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

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
        esp_err_t ret;
        for (size_t i = 0; i < LEN_OF(bda); i++)
        {
        	ret = esp_ble_gattc_open(
        		profile_tab[PROFILE_A_APP_ID].gattc_if,
        		bda[i],
        		BLE_ADDR_TYPE_PUBLIC,
        		true);

        	if (ret != ESP_OK)
        	{
        		ESP_LOGI(TAG, "Cannot connect to %x with: %x", bda[i][5], ret);
        	    break;
        	}

        	vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        break;
    }

    case ESP_GATTC_CONNECT_EVT:
    {
        ESP_LOGI(
        	TAG,
			"ESP_GATTC_CONNECT_EVT conn_id %d, if %d",
			param->connect.conn_id,
			gattc_if);
        profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
        memcpy(
        	profile_tab[PROFILE_A_APP_ID].remote_bda,
			param->connect.remote_bda,
			sizeof(esp_bd_addr_t));
        ESP_LOGI(
        	TAG,
			"REMOTE BDA:");
        esp_log_buffer_hex(
        	TAG,
			profile_tab[PROFILE_A_APP_ID].remote_bda,
			sizeof(esp_bd_addr_t));
        esp_err_t ret;
        if ((ret = esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id)) != ESP_OK)
        {
            ESP_LOGE(
            	TAG,
				"config MTU error, error code = %x",
				ret);
        }
        esp_ble_gattc_register_for_notify(
        	gattc_if,
			param->connect.remote_bda,
			0x2a);
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
        esp_ble_gattc_search_service(
        	gattc_if,
			param->cfg_mtu.conn_id,
			&remote_filter_service_uuid);
        break;

    case ESP_GATTC_SEARCH_RES_EVT:
    {
        ESP_LOGI(
        	TAG,
			"SEARCH RES: conn_id = %x is primary service %d",
			param->search_res.conn_id,
			param->search_res.is_primary);
        ESP_LOGI(
        	TAG,
			"start handle %d end handle %d current handle value %d",
			param->search_res.start_handle,
			param->search_res.start_handle,
			param->search_res.srvc_id.inst_id);
        if (param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 &&
        	param->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID)
        {
            ESP_LOGI(TAG, "service found");
            get_server = true;
            profile_tab[PROFILE_A_APP_ID].service_start_handle = param->search_res.start_handle;
            profile_tab[PROFILE_A_APP_ID].service_end_handle = param->search_res.end_handle;
            ESP_LOGI(
            	TAG,
				"UUID16: %x",
				param->search_res.srvc_id.uuid.uuid.uuid16);
        }
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (param->search_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(
            	TAG,
				"search service failed, error status = %x",
				param->search_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (get_server)
        {
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count(
            	gattc_if,
                param->search_cmpl.conn_id,
                ESP_GATT_DB_CHARACTERISTIC,
                profile_tab[PROFILE_A_APP_ID].service_start_handle,
                profile_tab[PROFILE_A_APP_ID].service_end_handle,
                INVALID_HANDLE,
                &count);
            if (status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0)
            {
            	esp_gattc_char_elem_t char_elem_result[count];

				ESP_LOGI(TAG, "attr count: %d", count);
				status = esp_ble_gattc_get_char_by_uuid(
					gattc_if,
					param->search_cmpl.conn_id,
					profile_tab[PROFILE_A_APP_ID].service_start_handle,
					profile_tab[PROFILE_A_APP_ID].service_end_handle,
					remote_filter_char_uuid,
					char_elem_result,
					&count);
				if (status != ESP_GATT_OK)
				{
					ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
				}

				ESP_LOGI(TAG, "char handle %x", char_elem_result->char_handle);
				rms_handle = char_elem_result->char_handle;
				if (status != ESP_GATT_OK)
				{
					ESP_LOGE(TAG, "esp_ble_gattc_read_char");
				}
            }
            else
            {
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;

    case ESP_GATTC_SRVC_CHG_EVT:
    {
        esp_bd_addr_t bda;
        memcpy(
        	bda,
			param->srvc_chg.remote_bda,
			sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }

    case ESP_GATTC_READ_CHAR_EVT:
    	if (param->read.status != ESP_GATT_OK)
    	{
    		ESP_LOGE(
    			TAG,
				"read char failed, error status = %x",
				param->read.status);
    		break;
    	}
    	ESP_LOGI(
    		TAG,
			"read rms from %d with value %d",
			param->read.conn_id,
			param->read.value[0]);
    	break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    	ESP_LOGI(TAG, "register for notify");
    	break;

    case ESP_GATTC_NOTIFY_EVT:
    	ESP_LOGI(
    		TAG,
			"notified from boi %d: %d",
			param->notify.conn_id,
			param->notify.value[0]);
    	rms[param->notify.conn_id] = param->notify.value[0];
    	handle_rms_notification();
    	break;

    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_server = false;
        ESP_LOGI(
        	TAG,
			"ESP_GATTC_DISCONNECT_EVT, reason = %d",
			param->disconnect.reason);
        break;

    default:
        break;
    }
}

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
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
    		profile_tab[param->reg.app_id].gattc_if = gattc_if;
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


    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
	for (int idx = 0; idx < PROFILE_NUM; idx++)
	{
		const bool is_none = gattc_if == ESP_GATT_IF_NONE;
		const bool is_this = gattc_if == profile_tab[idx].gattc_if;

		/* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
		if ((is_none || is_this) && profile_tab[idx].gattc_cb)
				profile_tab[idx].gattc_cb(event, gattc_if, param);
	}
}
