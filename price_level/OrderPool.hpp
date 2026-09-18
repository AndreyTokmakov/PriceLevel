/**============================================================================
Name        : OrderPool.hpp
Created on  : 18.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : OrderPool.hpp
============================================================================**/

#ifndef PRICELEVEL_ORDERPOOL_HPP
#define PRICELEVEL_ORDERPOOL_HPP

#include <numeric>
#include <vector>
#include <set>
#include <ranges>

#include "Order.hpp"

namespace order_pool
{
    using Order = common::Order;

    class OrderPool
    {
    public:
        using Index =  uint32_t;
        using size_type = size_t;
        static constexpr Index InvalidIndex = std::numeric_limits<Index>::max();

    private:
        struct Slot
        {   // storage обязательно должен быть первым полем.
            alignas(Order) std::byte storage[sizeof(Order)];
        };

        static_assert(offsetof(Slot, storage) == 0);
        static_assert(sizeof(Slot) == sizeof(Order));

    public:
        explicit OrderPool(const size_type initialCapacity): capacity { initialCapacity }
        {
            slots.resize(capacity);
            available.resize(capacity);
            std::iota(available.begin(), available.end(), 0);
        }

        OrderPool(const OrderPool&) = delete;
        OrderPool& operator=(const OrderPool&) = delete;

        OrderPool(OrderPool&&) = delete;
        OrderPool& operator=(OrderPool&&) = delete;

        ~OrderPool() noexcept
        {
            destroy_all();
        }

        template <typename... Args>
            requires std::is_nothrow_constructible_v<Order, Args...>
        [[nodiscard]]
        Order* create(Args&&... args)
        {
            if (available.empty()) {
                throw std::bad_alloc{};
            }

            const Index index = available.back();
            available.pop_back();

            Order* order = std::construct_at(object_from_slot(slots[index]), std::forward<Args>(args)...);
            return order;
        }

        void destroy(Order* order) noexcept
        {
            if (order == nullptr) {
                return;
            }

            const Index index = index_from_order(order);
            std::destroy_at(order);

            available.push_back(index);
        }

        void destroy_all() noexcept
        {
            const std::set<size_type> availableSet (available.cbegin() , available.cend());
            for (size_type idx = 0; idx < capacity; ++idx) {
                if (!availableSet.contains(idx)) {
                    std::destroy_at(object_from_slot(slots[idx]));
                }
            }
        }

        [[nodiscard]]
        size_type size() const noexcept {
            return capacity - available.size();
        }

        [[nodiscard]]
        constexpr size_type getCapacity() const noexcept {
            return capacity;
        }

    private:
        static Order* object_from_slot(Slot& slot) noexcept {
            return std::launder(reinterpret_cast<Order*>(slot.storage));
        }

        [[nodiscard]]
        Index index_from_order(const Order* order) const noexcept
        {
            const Slot* slot = reinterpret_cast<const Slot*>(order);
            const std::ptrdiff_t distance = slot - slots.data();
            return static_cast<Index>(distance);
        }

    private:
        std::vector<Slot> slots;
        std::vector<Index> available;

        size_type capacity { 0 };
    };
}

#endif //PRICELEVEL_ORDERPOOL_HPP
