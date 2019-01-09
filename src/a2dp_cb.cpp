// C includes
#include <cstdint>
#include <cstring>
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

namespace
{
    constexpr auto TAG = "A2DP_CB";

    esp_bd_addr_t peer_bda;
    std::uint32_t m_pkt_cnt = 0;

    void callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *a2d)
    {
        switch (event)
        {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
            {
                ESP_LOGI(TAG, "A2DP connected");
                glue::notify_a2dp_connected();
            }
            else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
            {
                ESP_LOGI(TAG, "A2DP disconnecting");
                glue::notify_a2dp_disconnecting();
            }
            else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
            {
                ESP_LOGI(TAG, "A2DP disconnected");
                std::memset(peer_bda, 0, sizeof(esp_bd_addr_t));
                glue::notify_a2dp_disconnected();
            }
            break;

        case ESP_A2D_AUDIO_STATE_EVT:
            if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
            {
                m_pkt_cnt = 0;
                glue::notify_a2dp_media_started();
            }
            else if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED)
            {
                glue::notify_a2dp_media_stopped();
            }
            break;

        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
            ESP_LOGI(TAG, "Media ctrl ack event");
            break;

        case ESP_A2D_AUDIO_CFG_EVT:
            ESP_LOGI(
                TAG,
                "A2DP audio stream configuration, codec type %d",
                a2d->audio_cfg.mcc.type);

            // for now only SBC stream is supported
            if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC)
            {
                auto sample_rate = 16000;
                const auto oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
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

        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
        }
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
            rms[switching::current_a2dp_idx] = a;
            switching::handle_rms_notification();
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
    }
}
