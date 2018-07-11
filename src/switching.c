#include "switching.h"
#include "gattc.h"
#include "tags.h"
#include "a2dp_cb.h"

int current_a2dp_idx = -1;
static bool first_time = false;

void switch_to_a2dp(size_t idx)
{
	esp_err_t ret;

	ESP_LOGI(SWITCHING_TAG, "SWITCH!");

	if ((ret = esp_ble_gap_disconnect(bda[idx])) != ESP_OK)
	{
		ESP_LOGE(
			SWITCHING_TAG,
			"Cannot disconnect BLE %d",
			ret);
		return;
	}

	if (!first_time)
	{
		if ((ret = esp_a2d_sink_disconnect(bda[current_a2dp_idx])) != ESP_OK)
		{
			ESP_LOGE(
				SWITCHING_TAG,
				"Cannot disconnect A2DP %d",
				ret);
			return;
		}

		first_time = true;
	}

	if ((ret = a2d_cb_connect(bda[idx])) != ESP_OK)
	{
		ESP_LOGE(
			SWITCHING_TAG,
			"Cannot connect A2DP %d",
			ret);
		return;
	}

	current_a2dp_idx = idx;
}

void handle_rms_notification()
{
	uint8_t max_rms = 0;
	size_t max_idx = 0;

	for (size_t i = 0; i < NUM_SERVERS; i++)
	{
		if (rms[i] > max_rms)
		{
			max_rms = rms[i];
			max_idx = i;
		}
	}

	ESP_LOGI(GATTC_TAG, "Max rms from %d: %d", max_idx, max_rms);

	if (max_rms > rms[current_a2dp_idx])
	{
		ESP_LOGI(
			SWITCHING_TAG,
			"Better than current, SSSswitching from %d to %d",
			current_a2dp_idx,
			max_idx);
		switch_to_a2dp(max_idx);
	}
}
