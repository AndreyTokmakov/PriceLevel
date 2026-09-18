/**============================================================================
Name        : OrderPool_Indexed.hpp
Created on  : 18.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : OrderPool_Indexed.hpp
============================================================================**/

#ifndef PRICELEVEL_ORDERPOOL_INDEXED_HPP
#define PRICELEVEL_ORDERPOOL_INDEXED_HPP

#include <array>
#include "Order.hpp"

namespace order_pool
{
    using Order = common::Order;

    template <std::size_t Capacity>
    class ReusableOrderPool
    {
        static_assert(Capacity > 0);

    private:

        struct Slot
        {
            alignas(Order) std::byte storage[sizeof(Order)];
            Slot* next_free = nullptr;
            bool occupied = false;
        };

    public:
        ReusableOrderPool() noexcept {
            initialize_free_list();
        }

        ReusableOrderPool(const ReusableOrderPool&) = delete;
        ReusableOrderPool& operator=( const ReusableOrderPool&) = delete;

        ReusableOrderPool(ReusableOrderPool&&) = delete;
        ReusableOrderPool& operator=(ReusableOrderPool&&) = delete;

        ~ReusableOrderPool() noexcept {
            destroy_all();
        }

        template <typename... Args>
        Order* create(Args&&... args)
        {
            if (free_head_ == nullptr) {
                throw std::bad_alloc{};
            }

            Slot* slot = free_head_;
            free_head_ = slot->next_free;

            Order* order = std::construct_at(order_from_slot(slot), std::forward<Args>(args)...);

            slot->next_free = nullptr;
            slot->occupied = true;

            ++size_;
            return order;
        }

        void destroy(Order* order) noexcept
        {
            if (order == nullptr) {
                return;
            }

            Slot* slot = slot_from_order(order);

            if (!slot->occupied) {
                return;
            }

            std::destroy_at(order);

            slot->occupied = false;
            slot->next_free = free_head_;
            free_head_ = slot;

            --size_;
        }

        void destroy_all() noexcept
        {
            for (Slot& slot : slots_) {
                if (!slot.occupied) {
                    continue;
                }

                std::destroy_at(order_from_slot(&slot));
                slot.occupied = false;
                slot.next_free = nullptr;
            }

            free_head_ = nullptr;
            size_ = 0;
        }

        [[nodiscard]]
        std::size_t size() const noexcept {
            return size_;
        }

        [[nodiscard]]
        static constexpr std::size_t capacity() noexcept {
            return Capacity;
        }

        [[nodiscard]]
        bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]]
        bool full() const noexcept {
            return size_ == Capacity;
        }

    private:

        void initialize_free_list() noexcept
        {
            for (std::size_t i = 0; i + 1 < Capacity; ++i) {
                slots_[i].next_free = &slots_[i + 1];
                slots_[i].occupied = false;
            }

            slots_[Capacity - 1].next_free = nullptr;
            slots_[Capacity - 1].occupied = false;

            free_head_ = &slots_[0];
        }

        [[nodiscard]]
        static Order* order_from_slot(Slot* slot) noexcept {
            return std::launder( reinterpret_cast<Order*>(slot->storage));
        }

        [[nodiscard]]
        static const Order* order_from_slot(const Slot* slot) noexcept {
            return std::launder(reinterpret_cast<const Order*>(slot->storage));
        }

        [[nodiscard]]
        static Slot* slot_from_order(Order* order) noexcept
        {
            auto* address = reinterpret_cast<std::byte*>(order);
            constexpr std::size_t storage_offset = offsetof(Slot, storage);
            return reinterpret_cast<Slot*>( address - storage_offset);
        }

    private:
        std::array<Slot, Capacity> slots_{};

        Slot* free_head_ = nullptr;
        std::size_t size_ = 0;
    };
}

#endif //PRICELEVEL_ORDERPOOL_INDEXED_HPP
