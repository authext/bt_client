#include <algorithm>
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
		const auto max_it = std::max_element(cbegin(rms), cend(rms));
		const auto max_rms = *max_it;
		const auto max_idx = std::distance(cbegin(rms), max_it);

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
				//glue::ble_to_a2dp(bda[current_a2dp_idx]);
				ESP_LOGI(TAG, "Would call BLE to A2DP for %d", max_idx);
			}
			else
			{
				int old_a2dp_idx = current_a2dp_idx;
				current_a2dp_idx = max_idx;
				/*glue::a2dp_to_a2dp(
					bda[old_a2dp_idx],
					bda[current_a2dp_idx]);*/
				ESP_LOGI(TAG, "Would call A2DP to A2DP for %d", max_idx);
			}
		}
		else if (rms[current_a2dp_idx] <= 2)
		{
			ESP_LOGI(TAG, "Would call A2DP to BLE for %d", current_a2dp_idx);
			//glue::a2dp_to_ble(bda[current_a2dp_idx]);
			current_a2dp_idx = -1;
		}
	}
}