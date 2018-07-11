#ifndef A2DP_CB_H
#define A2DP_CB_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "a2dp_core.h"


/* event for handler "bt_av_hdl_stack_up */
enum
{
    BT_APP_EVT_STACK_UP = 0,
};

/* A2DP global state */
enum
{
    APP_AV_STATE_IDLE,
    APP_AV_STATE_CONNECTING,
    APP_AV_STATE_CONNECTED,
    APP_AV_STATE_DISCONNECTING,
	APP_AV_STATE_DISCONNECTED,
};

/* sub states of APP_AV_STATE_CONNECTED */
enum
{
    APP_AV_MEDIA_STATE_IDLE,
    APP_AV_MEDIA_STATE_STARTING,
    APP_AV_MEDIA_STATE_STARTED,
    APP_AV_MEDIA_STATE_STOPPING,
};

#define BT_APP_HEART_BEAT_EVT 0xff00

/// handler for bluetooth stack enabled events
void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

/// A2DP application state machine
void bt_app_av_sm_hdlr(uint16_t event, void *param);


extern esp_bd_addr_t peer_bda;
extern int m_a2d_state;
extern int m_media_state;
extern int m_intv_cnt;
extern int m_connecting_intv;
extern uint32_t m_pkt_cnt;

extern TimerHandle_t tmr;

#endif
