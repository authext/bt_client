#include "switching.hpp"
#include "gattc.hpp"
#include "a2dp_cb.hpp"
#include "glue.hpp"

namespace
{
	constexpr auto TAG = "SWITCHING";
}

namespace switching
{
	int current_a2dp_idx = -1;

	void handle_rms_notification()
	{
		uint8_t max_rms = 0;
		size_t max_idx = 0;

		for (size_t i = 0; i < len_servers; i++)
		{
			if(len_servers >= MAX_NUM_SERVERS)
			{
				ESP_LOGI(TAG, "OMG! len_servers is %d.", len_servers);
			}
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

			if (current_a2dp_idx == -1)
			{
				current_a2dp_idx = max_idx;
				glue::ble_to_a2dp(bda[current_a2dp_idx]);
			}
			else
			{
				int old_a2dp_idx = current_a2dp_idx;
				current_a2dp_idx = max_idx;
				glue::a2dp_to_a2dp(
					bda[old_a2dp_idx],
					bda[current_a2dp_idx]);
			}
		}
		else if (rms[current_a2dp_idx] <= 2)
		{
			glue::a2dp_to_ble(bda[current_a2dp_idx]);
			current_a2dp_idx = -1;
		}
	}
}