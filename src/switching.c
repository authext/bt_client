#include "switching.h"
#include "gattc.h"
#include "tags.h"
#include "a2dp_cb.h"

int current_a2dp_idx = -1;

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
			"Better than current, switching from %d to %d",
			current_a2dp_idx,
			max_idx);
	}
}
