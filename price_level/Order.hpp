/**============================================================================
Name        : Order.hpp
Created on  : 18.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Order.hpp
============================================================================**/

#ifndef PRICELEVEL_ORDER_HPP
#define PRICELEVEL_ORDER_HPP

#include <cstdint>

namespace common
{
    struct Order
    {
        using OrderId = std::uint64_t;
        using Price = std::int64_t;
        using Quantity = std::int64_t;

        OrderId id { 0 };
        Price price { 0 };
        Quantity quantity { 0 };

        // Intrusive links.
        Order* prev { nullptr };
        Order* next { nullptr };

        Order(const OrderId id,
              const Price price,
              const Quantity quantity) noexcept:
            id(id),
            price(price),
            quantity(quantity)
        {
        }
    };
}

#endif //PRICELEVEL_ORDER_HPP
