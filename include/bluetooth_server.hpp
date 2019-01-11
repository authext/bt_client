#ifndef BLUETOOTH_SERVER_HPP
#define BLUETOOTH_SERVER_HPP

// C include
#include <cstdint>
// My includes
#include "bluetooth_address.hpp"

class bluetooth_server
{
public:
	/* Constructors */
	bluetooth_server(
		bluetooth_address address,
		std::uint16_t conn_id = 0,
		std::uint8_t activator = 0);
	bluetooth_server(const bluetooth_server&) = default;
	bluetooth_server(bluetooth_server&&) = default;

	/* Destructor */
	~bluetooth_server() = default;

	/* Operators */
	bluetooth_server& operator=(const bluetooth_server&) = default;
	bluetooth_server& operator=(bluetooth_server&&) = default;

	/* Getters */
	bluetooth_address& address();
	const bluetooth_address& address() const;

	std::uint8_t& activator();
	const std::uint8_t& activator() const;

	std::uint16_t& conn_id();
	const std::uint16_t& conn_id() const;

	/* Setters */
	bluetooth_address& address(bluetooth_address address);
	std::uint8_t& activator(std::uint8_t activator);
	std::uint16_t& conn_id(std::uint16_t conn_id);

private:
	bluetooth_address m_address;
	std::uint16_t m_conn_id;
	std::uint8_t m_activator;
};

bool operator==(const bluetooth_server& l, const bluetooth_server& r);
bool operator!=(const bluetooth_server& l, const bluetooth_server& r);

#endif
