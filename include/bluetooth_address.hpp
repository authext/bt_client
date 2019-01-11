#ifndef BLUETOOTH_ADDRESS_HPP
#define BLUETOOTH_ADDRESS_HPP

// C++ includes
#include <iosfwd>
// ESP includes
#include "esp_bt_device.h"

class bluetooth_address
{
public:
	/* Constructors */
    bluetooth_address(esp_bd_addr_t addr);
    bluetooth_address(const bluetooth_address&) = default;
    bluetooth_address(bluetooth_address&&) = default;

    /* Destructor */
    ~bluetooth_address() = default;

    /* Operators */
    bluetooth_address& operator=(const bluetooth_address&) = default;
    bluetooth_address& operator=(bluetooth_address&&) = default;

    /* Getters */
    esp_bd_addr_t& raw();
    const esp_bd_addr_t& raw() const;

    /* Implicit conversions */
    operator esp_bd_addr_t&();
    operator const esp_bd_addr_t&() const;

private:
    esp_bd_addr_t m_addr;
};

bool operator==(const bluetooth_address& l, const bluetooth_address& r);
bool operator!=(const bluetooth_address& l, const bluetooth_address& r);

std::ostream& operator<<(std::ostream& out, const bluetooth_address& addr);

#endif
