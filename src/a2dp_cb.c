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
#include "glue.h"
#include "switching.h"


#define BT_APP_HEART_BEAT_EVT 0xff00


typedef enum
{
    A2DP_CB_STATE_IDLE,
    A2DP_CB_STATE_CONNECTING,
    A2DP_CB_STATE_CONNECTED,
    A2DP_CB_STATE_DISCONNECTING,
} a2dp_cb_state_t;

/* Only valid when connected */
typedef enum
{
    A2DP_CB_MEDIA_STATE_IDLE,
    A2DP_CB_MEDIA_STATE_STARTING,
    A2DP_CB_MEDIA_STATE_STARTED,
    A2DP_CB_MEDIA_STATE_STOPPING,
} a2dp_cb_media_state;


/* A2DP application state machine handler for each state */
static void a2dp_cb_state_connecting(uint16_t event, void *param);
static void a2dp_cb_state_connected(uint16_t event, void *param);
static void a2dp_cb_state_disconnecting(uint16_t event, void *param);

static void a2dp_cb_heart_beat(void *arg);
/// callback function for A2DP source
static void a2dp_cb_cb(
	esp_a2d_cb_event_t event,
	esp_a2d_cb_param_t *param);
/// A2DP application state machine
static void a2dp_cb_state_machine(uint16_t event, void *param);

/// callback function for A2DP source audio data stream
static void a2dp_cb_data_cb(const uint8_t *data, uint32_t len);

static esp_bd_addr_t peer_bda;
static int m_a2d_state = A2DP_CB_STATE_IDLE;
static int m_media_state = A2DP_CB_MEDIA_STATE_IDLE;
static int m_connecting_intv = 0;
static uint32_t m_pkt_cnt = 0;

static TimerHandle_t tmr;


esp_err_t a2dp_cb_connect(const esp_bd_addr_t addr)
{
	memcpy(peer_bda, addr, sizeof(esp_bd_addr_t));

	m_a2d_state = A2DP_CB_STATE_CONNECTING;
	esp_err_t ret = esp_a2d_sink_connect(peer_bda);
	if (ret != ESP_OK)
		m_a2d_state = A2DP_CB_STATE_IDLE;
	return ret;
}

static void a2dp_cb_gap_cb(
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

void a2dp_cb_handle_stack_event(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CB_TAG, "%s evt %d", __func__, event);

    esp_err_t ret;

    switch (event)
    {
    case A2D_CB_EVENT_STACK_UP:
    {
    	ESP_LOGI(A2DP_CB_TAG, "Setting up A2DP");
    	esp_bt_dev_set_device_name("CLIENT");
        if ((ret = esp_bt_gap_register_callback(a2dp_cb_gap_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot register A2DP GAP callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_register_callback(a2dp_cb_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot regsiter A2DP callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_sink_register_data_callback(a2dp_cb_data_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot register A2DP data callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_sink_init()) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot init A2DP sink %d", ret);
        	return;
        }

        if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot set scan mode %d", ret);
        	return;
        }

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
	static uint32_t sum_len = 0;

	if (len == 0 || data == NULL)
		return;

	sum_len += len;

	if (++m_pkt_cnt % 100 == 0)
		ESP_LOGI(A2DP_CB_TAG, "RECEIVED PACKETS 0x%08x (0x%08x B)", m_pkt_cnt, sum_len);

	if (m_pkt_cnt % 1000 == 0)
	{
		int a = rand() % 4 + 1;
		printf("(A2DP) I have a rms of %d", a);
		if (a > 2)
			handle_rms_notification();
	}
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
    case A2DP_CB_STATE_IDLE:
        break;

    case A2DP_CB_STATE_CONNECTING:
        a2dp_cb_state_connecting(event, param);
        break;

    case A2DP_CB_STATE_CONNECTED:
        a2dp_cb_state_connected(event, param);
        break;

    case A2DP_CB_STATE_DISCONNECTING:
        a2dp_cb_state_disconnecting(event, param);
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
            ESP_LOGI(A2DP_CB_TAG, "A2DP connected");
            m_a2d_state =  A2DP_CB_STATE_CONNECTED;
            m_media_state = A2DP_CB_MEDIA_STATE_STARTING;
            glue_notify_a2dp_connected();
        }
        else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
        	memset(peer_bda, 0, sizeof(esp_bd_addr_t));
            m_a2d_state = A2DP_CB_STATE_IDLE;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;

    case ESP_A2D_AUDIO_CFG_EVT:
    	ESP_LOGI(
    		A2DP_CB_TAG,
    		"A2DP audio stream configuration, codec type %d",
			a2d->audio_cfg.mcc.type);

		// for now only SBC stream is supported
		if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC)
		{
			int sample_rate = 16000;
			char oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
			if (oct0 & (0x01 << 6))
				sample_rate = 32000;
			else if (oct0 & (0x01 << 5))
				sample_rate = 44100;
			else if (oct0 & (0x01 << 4))
				sample_rate = 48000;

			ESP_LOGI(A2DP_CB_TAG,
				"Configure audio player %02x-%02x-%02x-%02x",
				a2d->audio_cfg.mcc.cie.sbc[0],
				a2d->audio_cfg.mcc.cie.sbc[1],
				a2d->audio_cfg.mcc.cie.sbc[2],
				a2d->audio_cfg.mcc.cie.sbc[3]);
			ESP_LOGI(
				A2DP_CB_TAG,
				"Audio player configured, sample rate=%d",
				sample_rate);
		}
    	break;

    case BT_APP_HEART_BEAT_EVT:
        if (++m_connecting_intv >= 2)
        {
            m_a2d_state = A2DP_CB_STATE_IDLE;
            m_connecting_intv = 0;
            ESP_LOGW(A2DP_CB_TAG, "Failed to connect");
            esp_a2d_sink_disconnect(peer_bda);
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

    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
        {
            ESP_LOGI(A2DP_CB_TAG, "A2DP disconnecting");
            m_a2d_state = A2DP_CB_STATE_DISCONNECTING;
            glue_notify_a2dp_disconnecting();
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
        {
        	m_pkt_cnt = 0;
        }
        else if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED)
        {
        	glue_notify_a2dp_media_stopped();
        }

        break;

    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
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
            ESP_LOGI(A2DP_CB_TAG, "A2DP disconnected");
            m_a2d_state =  A2DP_CB_STATE_IDLE;
            glue_notify_a2dp_disconnected();
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
