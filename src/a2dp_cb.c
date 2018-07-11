#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "a2dp_core.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "tags.h"
#include "a2dp_cb.h"

/* A2DP application state machine handler for each state */
static void bt_app_av_state_connecting(uint16_t event, void *param);
static void bt_app_av_state_connected(uint16_t event, void *param);
static void bt_app_av_state_disconnecting(uint16_t event, void *param);
static void bt_app_av_state_disconnected(uint16_t event, void *param);

static void bt_app_av_media_proc(uint16_t event, void *param);
static void a2d_app_heart_beat(void *arg);
/// callback function for A2DP source
static void bt_app_a2d_cb(
	esp_a2d_cb_event_t event,
	esp_a2d_cb_param_t *param);

/// callback function for A2DP source audio data stream
static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);

esp_bd_addr_t peer_bda;
int m_a2d_state = APP_AV_STATE_IDLE;
int m_media_state = APP_AV_MEDIA_STATE_IDLE;
int m_intv_cnt = 0;
int m_connecting_intv = 0;
uint32_t m_pkt_cnt = 0;

TimerHandle_t tmr;


void bt_app_gap_cb(
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

void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CB_TAG, "%s evt %d", __func__, event);

    switch (event)
    {
    case BT_APP_EVT_STACK_UP:
    {
        esp_bt_gap_register_callback(bt_app_gap_cb);

        esp_a2d_register_callback(bt_app_a2d_cb);
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
        esp_a2d_sink_init();

        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_NONE);

        /* create and start heart beat timer */
        tmr = xTimerCreate(
        	"HEART_BEAT",
			(10000 / portTICK_RATE_MS),
            pdTRUE,
			NULL,
			a2d_app_heart_beat);
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

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    bt_app_work_dispatch(
    	bt_app_av_sm_hdlr,
		event,
		param,
		sizeof(esp_a2d_cb_param_t));
}

static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
	if (++m_pkt_cnt % 100 == 0)
		ESP_LOGI(A2DP_CB_TAG, "Received %u packets", m_pkt_cnt);
}

static void a2d_app_heart_beat(void *arg)
{
    bt_app_work_dispatch(
    	bt_app_av_sm_hdlr,
		BT_APP_HEART_BEAT_EVT,
		NULL,
		0);
}

void bt_app_av_sm_hdlr(uint16_t event, void *param)
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
        bt_app_av_state_connecting(event, param);
        break;

    case APP_AV_STATE_CONNECTED:
        bt_app_av_state_connected(event, param);
        break;

    case APP_AV_STATE_DISCONNECTING:
        bt_app_av_state_disconnecting(event, param);
        break;

    case APP_AV_STATE_DISCONNECTED:
    	bt_app_av_state_disconnected(event, param);
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

static void bt_app_av_state_connecting(uint16_t event, void *param)
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

static void bt_app_av_state_connected(uint16_t event, void *param)
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
        bt_app_av_media_proc(event, param);
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void bt_app_av_state_disconnecting(uint16_t event, void *param)
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

static void bt_app_av_state_disconnected(uint16_t event, void *param)
{

}

static void bt_app_av_media_proc(uint16_t event, void *param)
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
