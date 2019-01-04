#ifndef A2DP_CB_HPP
#define A2DP_CB_HPP

// C includes
#include <cstdint>

namespace a2dp_cb
{
	void init_stack(std::uint16_t event, void *param);
	esp_err_t connect(const esp_bd_addr_t addr);
}


#endif
