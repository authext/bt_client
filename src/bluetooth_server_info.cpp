// Matching include
#include "bluetooth_server_info.hpp"
// C++ includes
#include <utility>
// C includes
#include <cstdint>
// My includes
#include "bluetooth_address.hpp"

bluetooth_server_info::bluetooth_server_info(
	bluetooth_address address,
	std::uint16_t conn_id,
	std::uint8_t activator)
	: m_address(std::move(address))
	, m_conn_id(conn_id)
	, m_activator(activator)
{
}

bluetooth_address& bluetooth_server_info::address()
{
	return m_address;
}

const bluetooth_address& bluetooth_server_info::address() const
{
	return m_address;
}

std::uint8_t& bluetooth_server_info::activator()
{
	return m_activator;
}

const std::uint8_t& bluetooth_server_info::activator() const
{
	return m_activator;
}

std::uint16_t& bluetooth_server_info::conn_id()
{
	return m_conn_id;
}

const std::uint16_t& bluetooth_server_info::conn_id() const
{
	return m_conn_id;
}

bluetooth_address& bluetooth_server_info::address(bluetooth_address address)
{
	m_address = std::move(address);
	return m_address;
}

std::uint8_t& bluetooth_server_info::activator(std::uint8_t activator)
{
	m_activator = activator;
	return m_activator;
}

std::uint16_t& bluetooth_server_info::conn_id(std::uint16_t conn_id)
{
	m_conn_id = conn_id;
	return m_conn_id;
}

bool operator==(const bluetooth_server_info& l, const bluetooth_server_info& r)
{
	return l.address() == r.address()
		&& l.activator() == r.activator();
}

bool operator!=(const bluetooth_server_info& l, const bluetooth_server_info& r)
{
	return !(l == r);
}
