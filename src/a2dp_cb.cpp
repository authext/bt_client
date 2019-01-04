// C++ includes
#include <thread>
// C includes
#include <cstdint>
#include <cstring>
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
#include "a2dp_core.hpp"
#include "a2dp_cb.hpp"
#include "glue.hpp"
#include "switching.hpp"
#include "gattc.hpp"

using namespace std::literals;

namespace
{
    constexpr auto TAG = "A2DP_CB";

    constexpr auto BT_APP_HEART_BEAT_EVT = 0xff00;

    enum class state_t
    {
        IDLE,
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
    };

    enum class media_state_t
    {
        IDLE,
        STARTING,
        STARTED,
        STOPPING,
    };

    esp_bd_addr_t peer_bda;
    state_t m_a2d_state = state_t::IDLE;
    media_state_t m_media_state = media_state_t::IDLE;
    int m_connecting_intv = 0;
    std::uint32_t m_pkt_cnt = 0;

    void state_connecting(std::uint16_t event, void *param)
    {
        const auto *a2d = (esp_a2d_cb_param_t *)param;

        switch (event)
        {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
            {
                ESP_LOGI(TAG, "A2DP connected");
                m_a2d_state =  state_t::CONNECTED;
                m_media_state = media_state_t::STARTING;
                glue::notify_a2dp_connected();
            }
            else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
            {
                std::memset(peer_bda, 0, sizeof(esp_bd_addr_t));
                m_a2d_state = state_t::IDLE;
            }
            break;

        case ESP_A2D_AUDIO_STATE_EVT:
        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
            break;

        case ESP_A2D_AUDIO_CFG_EVT:
            ESP_LOGI(
                TAG,
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

                ESP_LOGI(TAG,
                    "Configure audio player %02x-%02x-%02x-%02x",
                    a2d->audio_cfg.mcc.cie.sbc[0],
                    a2d->audio_cfg.mcc.cie.sbc[1],
                    a2d->audio_cfg.mcc.cie.sbc[2],
                    a2d->audio_cfg.mcc.cie.sbc[3]);
                ESP_LOGI(
                    TAG,
                    "Audio player configured, sample rate=%d",
                    sample_rate);
            }
            break;

        case BT_APP_HEART_BEAT_EVT:
            if (++m_connecting_intv >= 2)
            {
                m_a2d_state = state_t::IDLE;
                m_connecting_intv = 0;
                ESP_LOGW(TAG, "Failed to connect");
                esp_a2d_sink_disconnect(peer_bda);
            }
            break;

        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
        }
    }

    void state_connected(std::uint16_t event, void *param)
    {
        const auto *a2d = (esp_a2d_cb_param_t *)param;

        switch (event)
        {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
            {
                ESP_LOGI(TAG, "A2DP disconnecting");
                m_a2d_state = state_t::DISCONNECTING;
                glue::notify_a2dp_disconnecting();
            }
            break;

        case ESP_A2D_AUDIO_STATE_EVT:
            if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
            {
                m_pkt_cnt = 0;
            }
            else if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED)
            {
                glue::notify_a2dp_media_stopped();
            }

            break;

        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        case BT_APP_HEART_BEAT_EVT:
            break;

        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
        }
    }

    void state_disconnecting(std::uint16_t event, void *param)
    {
        const auto *a2d = (esp_a2d_cb_param_t *)param;

        switch (event)
        {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
            {
                ESP_LOGI(TAG, "A2DP disconnected");
                m_a2d_state =  state_t::IDLE;
                glue::notify_a2dp_disconnected();
            }
            break;

        case ESP_A2D_AUDIO_STATE_EVT:
        case ESP_A2D_AUDIO_CFG_EVT:
        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        case BT_APP_HEART_BEAT_EVT:
            break;

        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
        }
    }

    void state_machine(std::uint16_t event, void *param)
    {
        ESP_LOGI(
            TAG,
            "%s state %d, evt 0x%x",
            __func__,
            static_cast<int>(m_a2d_state),
            event);

        switch (m_a2d_state)
        {
        case state_t::IDLE:
            m_connecting_intv = 0;
            break;

        case state_t::CONNECTING:
            state_connecting(event, param);
            break;

        case state_t::CONNECTED:
            state_connected(event, param);
            break;

        case state_t::DISCONNECTING:
            state_disconnecting(event, param);
            break;

        default:
            ESP_LOGE(
                TAG,
                "%s invalid state %d",
                __func__,
                static_cast<int>(m_a2d_state));
            break;
        }
    }

    void callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
    {
        a2dp_core::dispatch(
            state_machine,
            event,
            param,
            sizeof(esp_a2d_cb_param_t));
    }

    void data_callback(const std::uint8_t *data, std::uint32_t len)
    {
        static std::uint32_t sum_len = 0;

        if (len == 0 || data == nullptr)
            return;

        sum_len += len;

        if (++m_pkt_cnt % 100 == 0)
            ESP_LOGI(TAG, "RECEIVED PACKETS 0x%08x (0x%08x B)", m_pkt_cnt, sum_len);

        if (m_pkt_cnt % 1000 == 0)
        {
            int a = rand() % 4 + 1;
            printf("(A2DP) I have a rms of %d\n", a);
            rms[current_a2dp_idx] = a;
            handle_rms_notification();
        }
    }

    void gap_callback(
        esp_bt_gap_cb_event_t event,
        esp_bt_gap_cb_param_t *param)
    {
        switch (event)
        {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
            {
                ESP_LOGI(TAG, "authentication success: %s", param->auth_cmpl.device_name);
                esp_log_buffer_hex(TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            }
            else
            {
                ESP_LOGI(TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
            }
            break;

        default:
            ESP_LOGI(TAG, "event: %d", event);
            break;
        }
    }

    void heart_beat()
    {
        a2dp_core::dispatch(
            state_machine,
            BT_APP_HEART_BEAT_EVT,
            nullptr,
            0);
    }

    void on_timer(void (*action)())
    {
        for (;;)
        {
            std::this_thread::sleep_for(10s);
            action();
        }
    }
}

namespace a2dp_cb
{
    void init_stack(std::uint16_t, void *)
    {
    	ESP_LOGI(TAG, "Setting up A2DP");
    	esp_bt_dev_set_device_name("CLIENT");

        ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
        ESP_ERROR_CHECK(esp_a2d_register_callback(callback));
        ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(data_callback));
        ESP_ERROR_CHECK(esp_a2d_sink_init());
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE));

        std::thread(on_timer, heart_beat).detach();
    }

    esp_err_t connect(const esp_bd_addr_t addr)
    {
        std::memcpy(peer_bda, addr, sizeof(esp_bd_addr_t));

        m_a2d_state = state_t::CONNECTING;
        esp_err_t ret = esp_a2d_sink_connect(peer_bda);
        if (ret != ESP_OK)
            m_a2d_state = state_t::IDLE;
        return ret;
    }
}
