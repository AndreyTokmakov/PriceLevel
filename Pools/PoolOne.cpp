/**============================================================================
Name        : Pools.cpp
Created on  : 18.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Pools.cpp
============================================================================**/

#include "Pool.hpp"

#include <iostream>
#include <numeric>
#include <vector>
#include <set>
#include <ranges>

#include "Testing.hpp"

namespace
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

namespace
{

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

namespace
{
    using testing::Action;
    using testing::AssertEqual;
    using testing::AssertFalse;
    using testing::AssertIsNull;
    using testing::AssertNotNull;
    using testing::AssertTrue;

    void testEmptyPool()
    {
        OrderPool pool(4);

        AssertEqual(std::size_t { 0 }, pool.size(), "Pool must be empty");
        AssertEqual(std::size_t { 4 }, pool.getCapacity(), "Invalid pool capacity");
    }

    void testCreate()
    {
        OrderPool pool(4);

        const Order* order = pool.create(100, 12345, 500);

        AssertNotNull(order, "create() returned nullptr");

        AssertEqual(Order::OrderId { 100 }, order->id, "Invalid Order id");
        AssertEqual(Order::Price { 12345 }, order->price, "Invalid Order price");
        AssertEqual(Order::Quantity { 500 }, order->quantity, "Invalid Order quantity");

        AssertIsNull(order->prev, "prev must be initialized to nullptr");
        AssertIsNull(order->next, "next must be initialized to nullptr");

        AssertEqual(std::size_t { 1 }, pool.size(), "Invalid pool size");
    }

    void testCreateMultiple()
    {
        constexpr std::size_t Capacity = 4;

        OrderPool pool(Capacity);

        std::vector<Order*> orders;
        orders.reserve(Capacity);

        for (std::size_t i = 0; i < Capacity; ++i) {
            orders.push_back(pool.create(
                i + 1,
                static_cast<Order::Price>((i + 1) * 100),
                static_cast<Order::Quantity>((i + 1) * 10)));
        }

        AssertEqual(Capacity, orders.size(), "Invalid number of created orders");
        AssertEqual(Capacity, pool.size(), "Pool must be full");

        for (std::size_t i = 0; i < Capacity; ++i) {
            AssertNotNull(orders[i], "create() returned nullptr");
            AssertEqual(i + 1,orders[i]->id,"Invalid Order id");
        }
    }

    void testCapacity()
    {
        constexpr std::size_t Capacity = 3;

        OrderPool pool(Capacity);

        AssertEqual(Capacity, pool.getCapacity(), "Invalid capacity");
        AssertEqual(std::size_t { 0 }, pool.size(), "Invalid initial size");

        Order* first = pool.create(1, 100, 10);
        AssertNotNull(first);

        AssertEqual(std::size_t { 1 }, pool.size(), "Invalid size");
        Order* second = pool.create(2, 200, 20);
        AssertNotNull(second);

        AssertEqual(std::size_t { 2 }, pool.size(), "Invalid size");

        Order* third = pool.create(3, 300, 30);
        AssertNotNull(third);

        AssertEqual(Capacity, pool.size(), "Invalid size");
    }

    void testExhaustion()
    {
        OrderPool pool(2);

        Order* first = pool.create(1, 100, 10);
        Order* second = pool.create(2, 200, 20);

        AssertNotNull(first);
        AssertNotNull(second);
        AssertEqual(std::size_t { 2 }, pool.size(), "Pool must be full");

        bool badAllocThrown = false;

        try {
            [[maybe_unused]] Order* third = pool.create(3, 300, 30);
        }
        catch (const std::bad_alloc&) {
            badAllocThrown = true;
        }

        AssertTrue(badAllocThrown, "create() must throw std::bad_alloc when pool is full");
        AssertEqual(std::size_t { 2 }, pool.size(), "Pool size changed after failed create");
    }

    void testDestroy()
    {
        OrderPool pool(4);

        Order* order = pool.create(1, 100, 10);

        AssertNotNull(order);
        AssertEqual(std::size_t { 1 }, pool.size(), "Invalid size before destroy");

        pool.destroy(order);

        AssertEqual(std::size_t { 0 }, pool.size(), "Invalid size after destroy");
    }

    void testDestroyNull()
    {
        OrderPool pool(4);

        pool.destroy(nullptr);

        AssertEqual(std::size_t { 0 }, pool.size(), "Destroying nullptr changed pool size");

        Order* order = pool.create(1, 100, 10);

        AssertNotNull(order);

        pool.destroy(order);
        pool.destroy(nullptr);

        AssertEqual(std::size_t { 0 }, pool.size(), "Destroying nullptr changed pool size");
    }

    void testSlotReuse()
    {
        OrderPool pool(2);

        Order* first = pool.create(1, 100, 10);
        Order* second = pool.create(2, 200, 20);

        AssertNotNull(first);
        AssertNotNull(second);

        pool.destroy(first);

        AssertEqual(std::size_t { 1 }, pool.size(), "Invalid size after destroy");

        Order* reused = pool.create(3, 300, 30);

        AssertNotNull(reused, "Slot reuse returned nullptr");
        AssertEqual(first, reused, "Pool did not reuse released slot");

        AssertEqual(Order::OrderId { 3 }, reused->id, "Invalid reused Order id");
        AssertEqual(Order::Price { 300 }, reused->price, "Invalid reused Order price");
        AssertEqual(Order::Quantity { 30 }, reused->quantity, "Invalid reused Order quantity");

        AssertEqual(std::size_t { 2 }, pool.size(), "Invalid size after reuse");
    }

    void testLifoSlotReuse()
    {
        constexpr std::size_t Capacity = 4;

        OrderPool pool(Capacity);

        Order* orders[Capacity];

        for (std::size_t i = 0; i < Capacity; ++i) {
            orders[i] = pool.create(
                static_cast<Order::OrderId>(i + 1),
                static_cast<Order::Price>(i + 100),
                static_cast<Order::Quantity>(i + 10));

            AssertNotNull(orders[i]);
        }

        pool.destroy(orders[0]);
        pool.destroy(orders[1]);

        Order* firstReused = pool.create(100, 1000, 100);
        Order* secondReused = pool.create(200, 2000, 200);

        AssertEqual(orders[1],firstReused,"Pool must reuse the most recently released slot first");
        AssertEqual(orders[0],secondReused,"Pool must reuse released slots in LIFO order");
    }

    void testDestroyMultiple()
    {
        constexpr std::size_t Capacity = 5;

        OrderPool pool(Capacity);

        Order* orders[Capacity];

        for (std::size_t i = 0; i < Capacity; ++i) {
            orders[i] = pool.create(
                static_cast<Order::OrderId>(i),
                static_cast<Order::Price>(i * 100),
                static_cast<Order::Quantity>(i * 10));

            AssertNotNull(orders[i]);
        }

        pool.destroy(orders[1]);
        pool.destroy(orders[3]);

        AssertEqual(Capacity - 2,pool.size(),"Invalid size after destroying multiple orders");

        Order* firstReused = pool.create(10, 1000, 100);
        Order* secondReused = pool.create(20, 2000, 200);

        AssertEqual(orders[3],firstReused,"Invalid first reused slot");
        AssertEqual(orders[1],secondReused,"Invalid second reused slot");
        AssertEqual(Capacity, pool.size(), "Pool must be full");
    }

    void testIntrusiveLinks()
    {
        OrderPool pool(3);

        Order* first = pool.create(1, 100, 10);
        Order* second = pool.create(2, 200, 20);
        Order* third = pool.create(3, 300, 30);

        first->next = second;
        second->prev = first;
        second->next = third;
        third->prev = second;

        AssertIsNull(first->prev, "Invalid first->prev");
        AssertEqual(second, first->next, "Invalid first->next");
        AssertEqual(first, second->prev, "Invalid second->prev");
        AssertEqual(third, second->next, "Invalid second->next");
        AssertEqual(second, third->prev, "Invalid third->prev");
        AssertIsNull(third->next, "Invalid third->next");
    }

    void testReuseResetsIntrusiveLinks()
    {
        OrderPool pool(1);

        Order* first = pool.create(1, 100, 10);
        AssertNotNull(first);

        first->prev = reinterpret_cast<Order*>(0x1);
        first->next = reinterpret_cast<Order*>(0x2);

        pool.destroy(first);

        Order* reused = pool.create(2, 200, 20);

        AssertEqual(first, reused, "Slot was not reused");

        AssertIsNull(reused->prev, "Reused Order prev must be nullptr");
        AssertIsNull(reused->next, "Reused Order next must be nullptr");
    }

    void testDestroyAll()
    {
        constexpr std::size_t Capacity = 5;

        OrderPool pool(Capacity);

        Order* first = pool.create(1, 100, 10);
        Order* second = pool.create(2, 200, 20);
        Order* third = pool.create(3, 300, 30);

        AssertNotNull(first);
        AssertNotNull(second);
        AssertNotNull(third);

        pool.destroy(second);

        AssertEqual(std::size_t { 2 },pool.size(),"Invalid size before destroy_all");

        pool.destroy_all();

        AssertEqual(std::size_t { 2 },pool.size(),"destroy_all must not change available storage");

        Order* reused = pool.create(4, 400, 40);

        AssertNotNull(reused);
    }

    void testDestroyAllWithEmptyPool()
    {
        OrderPool pool(5);

        pool.destroy_all();

        AssertEqual(std::size_t { 0 },pool.size(),"destroy_all changed empty pool");
    }

    void testDestroyAllWithFullPool()
    {
        constexpr std::size_t Capacity = 5;

        OrderPool pool(Capacity);

        for (std::size_t i = 0; i < Capacity; ++i) {
            Order* order = pool.create(static_cast<Order::OrderId>(i),static_cast<Order::Price>(i),static_cast<Order::Quantity>(i));

            AssertNotNull(order);
        }

        AssertEqual(Capacity, pool.size(), "Pool must be full");

        pool.destroy_all();

        AssertEqual(Capacity,pool.size(),"destroy_all must not modify available storage");
    }

    void testDestroyAllWithPartiallyAvailablePool()
    {
        constexpr std::size_t Capacity = 6;

        OrderPool pool(Capacity);

        Order* orders[Capacity];

        for (std::size_t i = 0; i < Capacity; ++i) {
            orders[i] = pool.create(
                static_cast<Order::OrderId>(i),
                static_cast<Order::Price>(i * 10),
                static_cast<Order::Quantity>(i * 100));

            AssertNotNull(orders[i]);
        }

        pool.destroy(orders[1]);
        pool.destroy(orders[4]);

        AssertEqual(Capacity - 2,pool.size(),"Invalid size before destroy_all");

        pool.destroy_all();

        AssertEqual(Capacity - 2,pool.size(),"destroy_all changed pool state");

        Order* reused1 = pool.create(10, 1000, 100);
        Order* reused2 = pool.create(20, 2000, 200);

        AssertNotNull(reused1);
        AssertNotNull(reused2);
        AssertEqual(orders[4],reused1,"destroy_all corrupted available slots");
        AssertEqual(orders[1],reused2,"destroy_all corrupted available slots");
    }
}

void pools::pool_one::TestAll()
{
    testEmptyPool();
    testCreate();
    testCreateMultiple();
    testCapacity();
    testExhaustion();
    testDestroy();
    testDestroyNull();
    testSlotReuse();
    testLifoSlotReuse();
    testDestroyMultiple();
    testIntrusiveLinks();
    testReuseResetsIntrusiveLinks();
    testDestroyAll();
    testDestroyAllWithEmptyPool();
    testDestroyAllWithFullPool();
    testDestroyAllWithPartiallyAvailablePool();
}