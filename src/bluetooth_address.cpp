// Matching include
#include "bluetooth_address.hpp"
// C++ includes
#include <ostream>
// C includes
#include <cstdio>
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
	out << to_string(addr);
	return out;
}

std::string to_string(const bluetooth_address& addr)
{
	// Kill me
	char buffer[ESP_BD_ADDR_LEN * 2 + (ESP_BD_ADDR_LEN - 1) + 1] = "";

	static const auto sep = ":";
	std::sprintf(buffer, "%02x", addr[0]);
	for (auto i = 1; i < ESP_BD_ADDR_LEN; i++)
		std::sprintf(buffer + 2 + 3*(i - 1), "%s%02x", sep, addr[i]);

	return buffer;
}