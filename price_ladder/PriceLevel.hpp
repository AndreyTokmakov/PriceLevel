/**============================================================================
Name        : PriceLevel.hpp
Created on  : 17.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : PriceLevel.hpp
============================================================================**/

#ifndef PRICELEVEL_PRICELEVEL_HPP
#define PRICELEVEL_PRICELEVEL_HPP

#include <cstdint>
#include <cstddef>

namespace price_ladder
{
    using Timestamp = uint64_t;
    using Price     = uint64_t;
    using Volume    = uint64_t;
    using OrderId   = uint64_t;

    enum class OrderSide : uint8_t {
        Buy,
        Sell
    };

    template<typename T>
    struct IntrusiveLink
    {
        T* prev { nullptr };
        T* next { nullptr };
    };

    struct Order : IntrusiveLink<Order>
    {
        OrderId   orderId { 0 };
        Price     priceTick { 0 };
        Volume    volume { 0 };
        Timestamp timestampNs { 0 };
        OrderSide side { OrderSide::Buy };
        struct PriceLevel* level {};

        void* operator new(size_t size);
        void operator delete(void* ptr);
    };

    struct PriceLevel
    {
        Price    priceTick { 0 };
        Volume   totalVolume { 0 };
        Order*   head { nullptr };
        Order*   tail { nullptr };

        explicit PriceLevel(const Price price) noexcept: priceTick(price){
        }

        void addOrder(Order* order) noexcept;

        void removeOrder(Order* order) noexcept;

        [[nodiscard]]
        Order* getBestOrder() const noexcept {
            return head;
        }

        [[nodiscard]]
        bool isEmpty() const noexcept {
            return head == nullptr;
        }

        [[nodiscard]]
        bool hasBuyOrders() const noexcept {
            return hasOrdersOfSide<OrderSide::Buy>();
        }

        [[nodiscard]]
        bool hasSellOrders() const noexcept {
            return hasOrdersOfSide<OrderSide::Sell>();
        }

        [[nodiscard]]
        Volume getBuyVolume() const noexcept {
            return getVolumeBySide<OrderSide::Buy>();
        }

        [[nodiscard]]
        Volume getSellVolume() const noexcept {
            return getVolumeBySide<OrderSide::Sell>();
        }

    private:

        template<OrderSide Side>
        [[nodiscard]]
        bool hasOrdersOfSide() const noexcept
        {
            for (const Order* current = head; current != nullptr; ) {
                if (current->side == Side) {
                    return true;
                }
                current = current->next;
            }
            return false;
        }

        template<OrderSide Side>
        [[nodiscard]]
        Volume getVolumeBySide() const noexcept
        {
            Volume volume = 0;
            for (const Order* current = head; current != nullptr; ) {
                if (current->side == Side) {
                    volume += current->volume;
                }
                current = current->next;
            }
            return volume;
        }
    };
}



#endif //PRICELEVEL_PRICELEVEL_HPP
