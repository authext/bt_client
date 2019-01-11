// Matching include
#include "bluetooth_address.hpp"
// C++ includes
#include <ostream>
// C includes
#include <cstring>

bluetooth_address::bluetooth_address(esp_bd_addr_t addr)
{
    std::memcpy(m_addr, addr, sizeof(esp_bd_addr_t));
}

esp_bd_addr_t& bluetooth_address::raw()
{
    return m_addr;
}

const esp_bd_addr_t& bluetooth_address::raw() const
{
    return m_addr;
}

bluetooth_address::operator esp_bd_addr_t&()
{
	return m_addr;
}

bluetooth_address::operator const esp_bd_addr_t&() const
{
	return m_addr;
}

bool operator==(const bluetooth_address& l, const bluetooth_address& r)
{
    return std::memcmp(l, r, sizeof(esp_bd_addr_t)) == 0;
}

bool operator!=(const bluetooth_address& l, const bluetooth_address& r)
{
    return !(l == r);
}

std::ostream& operator<<(std::ostream& out, const bluetooth_address& addr)
{
	static const auto will_be_sep = ":";
	const auto *sep = "";

	for (auto i = 0; i < ESP_BD_ADDR_LEN; i++)
	{
		out << addr[i] << sep;
		sep = will_be_sep;
	}
	
	return out;
}
