/**============================================================================
Name        : PriceLevel.cpp
Created on  : 17.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : PriceLevel.cpp
============================================================================**/

#include "PriceLevel.hpp"

#include <chrono>
#include <vector>

namespace
{
    // Simple memory pool implementation for zero dynamic allocation on hot path
    class MemoryPool
    {
        static constexpr size_t PoolSize { 1024 * 1024 * 1024};
        static inline std::vector<char> pool;
        static inline std::vector<void*> free_list;

    public:

        static void* allocate(const size_t size)
        {
            if (free_list.empty()) {
                // In production, expand pool here
                return malloc(size);
            }
            void* ptr = free_list.back();
            free_list.pop_back();
            return ptr;
        }

        static void deallocate(void* ptr) {
            free_list.push_back(ptr);
        }
    };

    price_ladder::Timestamp getCurrentTimestamp() {
        return std::chrono::steady_clock::now().time_since_epoch().count();
    }
}

namespace price_ladder
{
    void* Order::operator new(const size_t size) {
        return MemoryPool::allocate(size);
    }

    void Order::operator delete(void* ptr) {
        MemoryPool::deallocate(ptr);
    }
}

namespace price_ladder
{
    void PriceLevel::addOrder(Order* order) noexcept
    {
        order->level = this;
        order->prev = tail;
        order->next = nullptr;

        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }

        tail = order;
        totalVolume += order->volume;
    }

    void PriceLevel::removeOrder(Order* order) noexcept
    {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }

        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }

        totalVolume -= order->volume;
        order->level = nullptr;
        order->prev = nullptr;
        order->next = nullptr;
    }
}
