// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Unix includes
#include <unistd.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
// ESP System includes
#include "esp_system.h"
// Logging includes
#include "esp_log.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
// My includes
#include "a2dp_core.h"
#include "a2dp_cb.h"
#include "tags.h"


#define BT_APP_HEART_BEAT_EVT 0xff00


typedef enum
{
    APP_AV_STATE_IDLE,
    APP_AV_STATE_CONNECTING,
    APP_AV_STATE_CONNECTED,
    APP_AV_STATE_DISCONNECTING,
	APP_AV_STATE_DISCONNECTED,
} a2dp_cb_state_t;

/* Only valid when connected */
typedef enum
{
    APP_AV_MEDIA_STATE_IDLE,
    APP_AV_MEDIA_STATE_STARTING,
    APP_AV_MEDIA_STATE_STARTED,
    APP_AV_MEDIA_STATE_STOPPING,
} a2dp_cb_media_state;


/* A2DP application state machine handler for each state */
static void a2dp_cb_state_connecting(uint16_t event, void *param);
static void a2dp_cb_state_connected(uint16_t event, void *param);
static void a2dp_cb_state_disconnecting(uint16_t event, void *param);
static void a2dp_cb_state_disconnected(uint16_t event, void *param);

static void a2dp_cb_media_proc(uint16_t event, void *param);
static void a2dp_cb_heart_beat(void *arg);
/// callback function for A2DP source
static void a2dp_cb_cb(
	esp_a2d_cb_event_t event,
	esp_a2d_cb_param_t *param);
/// A2DP application state machine
static void a2dp_cb_state_machine(uint16_t event, void *param);

/// callback function for A2DP source audio data stream
static void a2dp_cb_data_cb(const uint8_t *data, uint32_t len);

esp_bd_addr_t peer_bda;
int m_a2d_state = APP_AV_STATE_IDLE;
int m_media_state = APP_AV_MEDIA_STATE_IDLE;
int m_intv_cnt = 0;
int m_connecting_intv = 0;
uint32_t m_pkt_cnt = 0;

TimerHandle_t tmr;


void a2dp_cb_gap_cb(
	esp_bt_gap_cb_event_t event,
	esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(A2DP_CB_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            esp_log_buffer_hex(A2DP_CB_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        }
        else
        {
            ESP_LOGI(A2DP_CB_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;

    default:
        ESP_LOGI(A2DP_CB_TAG, "event: %d", event);
        break;
    }
}

void a2d_cb_handle_stack_event(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CB_TAG, "%s evt %d", __func__, event);

    switch (event)
    {
    case BT_APP_EVT_STACK_UP:
    {
        esp_bt_gap_register_callback(a2dp_cb_gap_cb);

        esp_a2d_register_callback(a2dp_cb_cb);
        esp_a2d_sink_register_data_callback(a2dp_cb_data_cb);
        esp_a2d_sink_init();

        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_NONE);

        /* create and start heart beat timer */
        tmr = xTimerCreate(
        	"HEART_BEAT",
			(10000 / portTICK_RATE_MS),
            pdTRUE,
			NULL,
			a2dp_cb_heart_beat);
        xTimerStart(tmr, portMAX_DELAY);
        break;
    }

    default:
        ESP_LOGE(
        	A2DP_CB_TAG,
			"%s unhandled evt %d",
			__func__,
			event);
        break;
    }
}

static void a2dp_cb_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    a2dp_core_dispatch(
    	a2dp_cb_state_machine,
		event,
		param,
		sizeof(esp_a2d_cb_param_t));
}

static void a2dp_cb_data_cb(const uint8_t *data, uint32_t len)
{
	if (++m_pkt_cnt % 100 == 0)
		ESP_LOGI(A2DP_CB_TAG, "Received %u packets", m_pkt_cnt);
}

static void a2dp_cb_heart_beat(void *arg)
{
    a2dp_core_dispatch(
    	a2dp_cb_state_machine,
		BT_APP_HEART_BEAT_EVT,
		NULL,
		0);
}

void a2dp_cb_state_machine(uint16_t event, void *param)
{
    ESP_LOGI(
    	A2DP_CB_TAG,
		"%s state %d, evt 0x%x",
		__func__,
		m_a2d_state,
		event);

    switch (m_a2d_state)
    {
    case APP_AV_STATE_IDLE:
        break;

    case APP_AV_STATE_CONNECTING:
        a2dp_cb_state_connecting(event, param);
        break;

    case APP_AV_STATE_CONNECTED:
        a2dp_cb_state_connected(event, param);
        break;

    case APP_AV_STATE_DISCONNECTING:
        a2dp_cb_state_disconnecting(event, param);
        break;

    case APP_AV_STATE_DISCONNECTED:
    	a2dp_cb_state_disconnected(event, param);
    	break;

    default:
        ESP_LOGE(
        	A2DP_CB_TAG,
			"%s invalid state %d",
			__func__,
			m_a2d_state);
        break;
    }
}

static void a2dp_cb_state_connecting(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)param;

    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp connected");
            m_a2d_state =  APP_AV_STATE_CONNECTED;
            m_media_state = APP_AV_MEDIA_STATE_IDLE;
        }
        else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
            m_a2d_state =  APP_AV_STATE_DISCONNECTED;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;

    case BT_APP_HEART_BEAT_EVT:
        if (++m_connecting_intv >= 2)
        {
            m_a2d_state = APP_AV_STATE_DISCONNECTED;
            m_connecting_intv = 0;
        }
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void a2dp_cb_state_connected(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)param;

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp disconnected");
            m_a2d_state = APP_AV_STATE_DISCONNECTED;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
            m_pkt_cnt = 0;
        break;

    case ESP_A2D_AUDIO_CFG_EVT:
        // TODO xxxx not suppposed to occur for A2DP source
        break;

    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
        a2dp_cb_media_proc(event, param);
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void a2dp_cb_state_disconnecting(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)param;

    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp disconnected");
            m_a2d_state =  APP_AV_STATE_DISCONNECTED;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void a2dp_cb_state_disconnected(uint16_t event, void *param)
{

}

static void a2dp_cb_media_proc(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)param;

    switch (m_media_state)
    {
    case APP_AV_MEDIA_STATE_IDLE:
        if (event == BT_APP_HEART_BEAT_EVT)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp media ready checking ...");
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        }
        else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        {
        	const bool is_cmd_ready = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY;
        	const bool is_status_success = a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;
            if (is_cmd_ready && is_status_success)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media ready, starting ...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                m_media_state = APP_AV_MEDIA_STATE_STARTING;
            }
        }
        break;

    case APP_AV_MEDIA_STATE_STARTING:
        if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        {
        	const bool is_cmd_start = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START;
        	const bool is_status_success = a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;
            if (is_cmd_start && is_status_success)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media start successfully.");
                m_intv_cnt = 0;
                m_media_state = APP_AV_MEDIA_STATE_STARTED;
            }
            else
            {
                // not started succesfully, transfer to idle state
                ESP_LOGI(A2DP_CB_TAG, "a2dp media start failed.");
                m_media_state = APP_AV_MEDIA_STATE_IDLE;
            }
        }
        break;

    case APP_AV_MEDIA_STATE_STARTED:
        if (event == BT_APP_HEART_BEAT_EVT)
        {
            if (++m_intv_cnt >= 10)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media stopping...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
                m_media_state = APP_AV_MEDIA_STATE_STOPPING;
                m_intv_cnt = 0;
            }
        }
        break;

    case APP_AV_MEDIA_STATE_STOPPING:
        if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        {
        	const bool is_cmd_stop = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_STOP;
        	const bool is_status_success = a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;

            if (is_cmd_stop && is_status_success)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media stopped successfully, disconnecting...");
                m_media_state = APP_AV_MEDIA_STATE_IDLE;
                esp_a2d_sink_disconnect(peer_bda);
                m_a2d_state = APP_AV_STATE_DISCONNECTING;
            }
            else
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media stopping...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
            }
        }
        break;
    }
}
