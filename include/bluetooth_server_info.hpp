#ifndef BLUETOOTH_SERVER_HPP
#define BLUETOOTH_SERVER_HPP

// C include
#include <cstdint>
// My includes
#include "bluetooth_address.hpp"

class bluetooth_server_info
{
public:
	/* Constructors */
	bluetooth_server_info(
		bluetooth_address address,
		std::uint16_t conn_id = 0,
		std::uint8_t activator = 0);
	bluetooth_server_info(const bluetooth_server_info&) = default;
	bluetooth_server_info(bluetooth_server_info&&) = default;

	/* Destructor */
	~bluetooth_server_info() = default;

	/* Operators */
	bluetooth_server_info& operator=(const bluetooth_server_info&) = default;
	bluetooth_server_info& operator=(bluetooth_server_info&&) = default;

	/* Getters */
	const bluetooth_address& address() const;
	bluetooth_address& address();
	const std::uint8_t& activator() const;
	std::uint8_t& activator();
	const std::uint16_t& conn_id() const;
	std::uint16_t& conn_id();
	const bool& ble_connected() const;
	bool& ble_connected();

private:
	bluetooth_address m_address;
	std::uint16_t m_conn_id;
	std::uint8_t m_activator;
	bool m_ble_connected;
};

bool operator==(const bluetooth_server_info& l, const bluetooth_server_info& r);
bool operator!=(const bluetooth_server_info& l, const bluetooth_server_info& r);

#endif
