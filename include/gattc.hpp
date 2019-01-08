#ifndef GATT_HPP
#define GATT_HPP

// C++ includes
#include <vector>
// C includes
#include <cstdint>
#include <cstring>
// Logging includes
#include "esp_log.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"


extern std::uint16_t interface;
extern std::uint16_t conn_id;
extern std::uint8_t current_rms;

class bd_address
{
public:
    bd_address(esp_bd_addr_t addr)
    {
        std::memcpy(m_addr, addr, sizeof(esp_bd_addr_t));
    }

    esp_bd_addr_t& addr()
    {
        return m_addr;
    }

    const esp_bd_addr_t& addr() const
    {
        return m_addr;
    }

    operator esp_bd_addr_t&()
    {
        return m_addr;
    }

private:
    esp_bd_addr_t m_addr;
};

inline bool operator==(const bd_address& l, const bd_address& r)
{
    return std::memcmp(l.addr(), r.addr(), sizeof(esp_bd_addr_t)) == 0;
}

extern std::vector<bd_address> bda;
extern std::vector<std::uint8_t> rms;


namespace gattc
{
	void esp_gap_cb(
		esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param);
	void esp_gattc_cb(
		esp_gattc_cb_event_t event,
		esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param);
}

#endif
