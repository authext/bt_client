// Matching include
#include "bluetooth_server.hpp"
// C++ includes
#include <utility>
// C includes
#include <cstdint>
// My includes
#include "bluetooth_address.hpp"

bluetooth_server::bluetooth_server(
	bluetooth_address address,
	std::uint16_t conn_id,
	std::uint8_t activator)
	: m_address(std::move(address))
	, m_conn_id(conn_id)
	, m_activator(activator)
{
}

bluetooth_address& bluetooth_server::address()
{
	return m_address;
}

const bluetooth_address& bluetooth_server::address() const
{
	return m_address;
}

std::uint8_t& bluetooth_server::activator()
{
	return m_activator;
}

const std::uint8_t& bluetooth_server::activator() const
{
	return m_activator;
}

std::uint16_t& bluetooth_server::conn_id()
{
	return m_conn_id;
}

const std::uint16_t& bluetooth_server::conn_id() const
{
	return m_conn_id;
}

bluetooth_address& bluetooth_server::address(bluetooth_address address)
{
	m_address = std::move(address);
	return m_address;
}

std::uint8_t& bluetooth_server::activator(std::uint8_t activator)
{
	m_activator = activator;
	return m_activator;
}

std::uint16_t& bluetooth_server::conn_id(std::uint16_t conn_id)
{
	m_conn_id = conn_id;
	return m_conn_id;
}

bool operator==(const bluetooth_server& l, const bluetooth_server& r)
{
	return l.address() == r.address()
		&& l.activator() == r.activator();
}

bool operator!=(const bluetooth_server& l, const bluetooth_server& r)
{
	return !(l == r);
}
