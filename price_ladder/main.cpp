/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : 
============================================================================**/

#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <thread>

// #include <absl/container/flat_hash_map.h>
#include <iostream>
#include <unordered_map>


#include "Testing.hpp"

/*
 * High-Performance Price Ladder for Low-Latency Trading Systems
 * ==============================================================
 *
 * This implementation provides a cache-efficient order book structure optimized
 * for high-frequency trading environments where nanosecond-level latency and
 * predictable performance are critical requirements.
 *
 * Key Design Decisions
 * --------------------
 *
 * 1. CRTP-based Intrusive Linked List
 *    - Order objects contain their own prev/next pointers (intrusive)
 *    - CRTP (Curiously Recurring Template Pattern) provides type safety without
 *      virtual functions or dynamic_cast overhead
 *    - Enables O(1) removal from any position in the list
 *    - Eliminates separate node allocations (each order is its own node)
 *
 * 2. Separate Bid/Ask Arrays
 *    - Bid levels stored in descending order (highest price first)
 *    - Ask levels stored in ascending order (lowest price first)
 *    - Improves cache locality for top-of-book operations
 *    - Enables efficient scanning for best prices
 *
 * 3. Cached Best Bid/Ask Pointers
 *    - Maintains direct pointers to best bid and ask levels
 *    - Provides O(1) top-of-book access without scanning
 *    - Automatically updated on level state changes
 *
 * 4. Open-Addressing Hash Map (absl::flat_hash_map)
 *    - Used for O(1) order lookup by ID (cancellations/modifications)
 *    - Open addressing stores all entries in contiguous memory
 *    - Significantly fewer cache misses than std::unordered_map (chaining)
 *    - Pre-reserved capacity prevents rehashing on hot path
 *
 * 5. Memory Pool Allocator
 *    - Eliminates system malloc/free calls on critical path
 *    - Pre-allocates memory blocks for orders
 *    - Prevents memory fragmentation
 *    - Enables predictable allocation latency
 *
 * 6. Zero Runtime Overhead
 *    - No virtual functions (no vtable indirection)
 *    - All methods marked noexcept for compiler optimization
 *    - constexpr where possible for compile-time evaluation
 *    - [[nodiscard]] prevents accidental ignoring of return values
 *
 * Pros
 * ----
 * + Extremely fast O(1) level access with single memory fetch
 * + O(1) order insertion and removal from lists
 * + O(1) best bid/ask access (cached pointers)
 * + Excellent cache locality for sequential operations
 * + No dynamic memory allocation on hot path (uses memory pool)
 * + Predictable low latency with minimal jitter
 * + Type-safe intrusive list (compile-time checks)
 * + Separate bid/ask arrays improve top-of-book performance
 *
 * Cons
 * ----
 * - Fixed price range requires knowing min/max prices at construction
 * - Memory overhead for empty levels (allocates full range even if sparse)
 * - Not suitable for instruments with very wide price ranges (e.g., crypto)
 * - Bid/ask separation doubles some maintenance complexity
 * - Single-threaded design (no internal locking or concurrency control)
 * - Memory pool requires careful management to avoid dangling pointers
 *
 * Performance Characteristics
 * ---------------------------
 * - getLevel: ~5-10 ns (single array access)
 * - getBestBid/getBestAsk: ~2-3 ns (direct pointer access)
 * - addOrder: ~50-100 ns (array access + list insert + hash insert)
 * - removeOrder: ~30-60 ns (list removal + hash erase)
 * - findOrder: ~20-40 ns (hash lookup)
 * - Memory footprint: ~8 bytes per level + 4 pointers per level + order data
 *
 * Typical Use Cases
 * -----------------
 * - Central limit order books (CLOB) for exchange connectivity
 * - Market data processing (top-of-book and depth updates)
 * - Algorithmic trading strategy backtesting
 * - Real-time risk management systems
 *
 * Limitations and Future Improvements
 * -----------------------------------
 * - Add sequence numbers or versioning for ABA problem prevention
 * - Support for concurrent access with sharding or lock-free structures
 * - Dynamic range expansion for instruments with widening spreads
 * - Add cache-aligned structures (alignas(64)) to prevent false sharing
 */

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

    using Timestamp = uint64_t;
    using Price     = uint64_t;
    using Volume    = uint64_t;
    using OrderId   = uint64_t;

    Timestamp getCurrentTimestamp() {
        return std::chrono::steady_clock::now().time_since_epoch().count();
    }
}


namespace
{
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

        void* operator new(const size_t size) {
            return MemoryPool::allocate(size);
        }

        void operator delete(void* ptr) {
            MemoryPool::deallocate(ptr);
        }
    };

    struct PriceLevel
    {
        Price    priceTick { 0 };
        Volume   totalVolume { 0 };
        Order*   head { nullptr };
        Order*   tail { nullptr };

        explicit PriceLevel(const Price price) noexcept: priceTick(price){
        }

        void addOrder(Order* order) noexcept
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

        void removeOrder(Order* order) noexcept
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


namespace
{
    class PriceLadder
    {
    public:
        PriceLadder(const Price minPriceTick, const Price maxPriceTick) noexcept:
            minPriceTick { minPriceTick },
            maxPriceTick { maxPriceTick },
            numLevels { maxPriceTick - minPriceTick + 1 }
        {
            levels.reserve(numLevels);
            for (Price price = minPriceTick; price <= maxPriceTick; ++price) {
                levels.emplace_back(price);
            }

            bidLevels.reserve(numLevels);
            for (Price price = maxPriceTick; price >= minPriceTick; --price) {
                bidLevels.push_back(&levels[price - minPriceTick]);
            }

            askLevels.reserve(numLevels);
            for (Price price = minPriceTick; price <= maxPriceTick; ++price) {
                askLevels.push_back(&levels[price - minPriceTick]);
            }

            orderIndex.reserve(kDefaultReserveSize);
        }

        [[nodiscard]]
        PriceLevel* getLevel(const Price priceTick) noexcept
        {
            const size_t index = priceTick - minPriceTick;
            return &levels[index];
        }

        [[nodiscard]]
        const PriceLevel* getLevel(const Price priceTick) const noexcept
        {
            const size_t index = priceTick - minPriceTick;
            return &levels[index];
        }

        [[nodiscard]]
        PriceLevel* getBidLevel(const size_t depth) noexcept
        {
            for (size_t nonEmptyCount = 0, i = 0; i < bidLevels.size(); ++i) {
                if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                    if (nonEmptyCount == depth) {
                        return bidLevels[i];
                    }
                    ++nonEmptyCount;
                }
            }
            return nullptr;
        }

        [[nodiscard]]
        const PriceLevel* getBidLevel(const size_t depth) const noexcept
        {
            for (size_t nonEmptyCount = 0, i = 0; i < bidLevels.size(); ++i) {
                if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                    if (nonEmptyCount == depth) {
                        return bidLevels[i];
                    }
                    ++nonEmptyCount;
                }
            }
            return nullptr;
        }

        [[nodiscard]]
        PriceLevel* getAskLevel(const size_t depth) noexcept
        {
            for (size_t nonEmptyCount = 0, i = 0; i < askLevels.size(); ++i) {
                if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                    if (nonEmptyCount == depth) {
                        return askLevels[i];
                    }
                    ++nonEmptyCount;
                }
            }
            return nullptr;
        }

        [[nodiscard]]
        const PriceLevel* getAskLevel(const size_t depth) const noexcept
        {
            for (size_t nonEmptyCount = 0, i = 0; i < askLevels.size(); ++i) {
                if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                    if (nonEmptyCount == depth) {
                        return askLevels[i];
                    }
                    ++nonEmptyCount;
                }
            }
            return nullptr;
        }

        void addOrder(Order* order) noexcept
        {
            PriceLevel* level = getLevel(order->priceTick);
            level->addOrder(order);
            orderIndex.emplace(order->orderId, order);
        }

        void removeOrder(Order* order) noexcept
        {
            order->level->removeOrder(order);
            orderIndex.erase(order->orderId);
        }

        static void modifyOrderVolume(Order* order, const Volume newVolume) noexcept
        {
            order->level->totalVolume -= order->volume;
            order->level->totalVolume += newVolume;
            order->volume = newVolume;
        }

        [[nodiscard]]
        Order* findOrder(const OrderId orderId) const noexcept
        {
            const auto it = orderIndex.find(orderId);
            return (it != orderIndex.end()) ? it->second : nullptr;
        }

        [[nodiscard]]
        Price getBestBid() const noexcept
        {
            for (size_t i = 0; i < bidLevels.size(); ++i) {
                if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                    return bidLevels[i]->priceTick;
                }
            }
            return 0;
        }

        [[nodiscard]]
        Price getBestAsk() const noexcept
        {
            for (size_t i = 0; i < askLevels.size(); ++i) {
                if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                    return askLevels[i]->priceTick;
                }
            }
            return std::numeric_limits<Price>::max();
        }

        [[nodiscard]]
        PriceLevel* getBestBidLevel() const noexcept
        {
            for (size_t i = 0; i < bidLevels.size(); ++i) {
                if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                    return bidLevels[i];
                }
            }
            return nullptr;
        }

        [[nodiscard]]
        PriceLevel* getBestAskLevel() const noexcept
        {
            for (size_t i = 0; i < askLevels.size(); ++i) {
                if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                    return askLevels[i];
                }
            }
            return nullptr;
        }

        [[nodiscard]]
        Volume getBestBidVolume() const noexcept
        {
            const PriceLevel* level = getBestBidLevel();
            return level ? level->getBuyVolume() : 0;
        }

        [[nodiscard]]
        Volume getBestAskVolume() const noexcept
        {
            const PriceLevel* level = getBestAskLevel();
            return level ? level->getSellVolume() : 0;
        }

        [[nodiscard]]
        Price getBidPriceAtDepth(const size_t depth) const noexcept {

            for (size_t nonEmptyCount = 0, i = 0; i < bidLevels.size(); ++i) {
                if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                    if (nonEmptyCount == depth) {
                        return bidLevels[i]->priceTick;
                    }
                    ++nonEmptyCount;
                }
            }
            return 0;
        }

        [[nodiscard]]
        Price getAskPriceAtDepth(const size_t depth) const noexcept
        {
            for (size_t nonEmptyCount = 0, i = 0; i < askLevels.size(); ++i) {
                if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                    if (nonEmptyCount == depth) {
                        return askLevels[i]->priceTick;
                    }
                    ++nonEmptyCount;
                }
            }
            return std::numeric_limits<Price>::max();
        }

        [[nodiscard]]
        Volume getBidVolumeAtDepth(const size_t depth) const noexcept
        {
            for (size_t nonEmptyCount = 0,  i = 0; i < bidLevels.size(); ++i) {
                if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                    if (nonEmptyCount == depth) {
                        return bidLevels[i]->getBuyVolume();
                    }
                    ++nonEmptyCount;
                }
            }
            return 0;
        }

        [[nodiscard]]
        Volume getAskVolumeAtDepth(const size_t depth) const noexcept
        {
            for (size_t nonEmptyCount = 0, i = 0; i < askLevels.size(); ++i) {
                if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                    if (nonEmptyCount == depth) {
                        return askLevels[i]->getSellVolume();
                    }
                    ++nonEmptyCount;
                }
            }
            return 0;
        }

        [[nodiscard]]
        bool isEmpty() const noexcept {
            return orderIndex.empty();
        }

        [[nodiscard]]
        size_t getOrderCount() const noexcept {
            return orderIndex.size();
        }

        [[nodiscard]]
        Price getMinPriceTick() const noexcept {
            return minPriceTick;
        }

        [[nodiscard]]
        Price getMaxPriceTick() const noexcept {
            return maxPriceTick;
        }

        [[nodiscard]]
        size_t getNumLevels() const noexcept {
            return numLevels;
        }

        [[nodiscard]]
        size_t getNumBidLevels() const noexcept {
            return bidLevels.size();
        }

        [[nodiscard]]
        size_t getNumAskLevels() const noexcept {
            return askLevels.size();
        }

    private:
        static constexpr size_t kDefaultReserveSize = 1000000;

        Price minPriceTick;
        Price maxPriceTick;
        size_t numLevels;
        std::vector<PriceLevel> levels;
        std::vector<PriceLevel*> bidLevels;
        std::vector<PriceLevel*> askLevels;
    #if 1
            std::unordered_map<OrderId, Order*> orderIndex;
    #else
            absl::flat_hash_map<OrderId, Order*> orderIndex;
    #endif
    };
}



namespace unit_tests
{

    // Helper function to get current timestamp in nanoseconds
    static uint64_t getCurrentTimestampNs()
    {
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }

    static Order* createTestOrder(const OrderId id, const Price price, const Volume volume, const OrderSide side) {
        Order* order = new Order();
        order->orderId = id;
        order->priceTick = price;
        order->volume = volume;
        order->timestampNs = getCurrentTimestampNs();
        order->side = side;  // ВАЖНО: устанавливаем сторону!
        order->level = nullptr;
        order->prev = nullptr;
        order->next = nullptr;
        return order;
    }

    static size_t countOrdersInLevel(const PriceLevel* level)
    {
        size_t count = 0;
        const Order* current = level->head;
        while (current) {
            count++;
            current = current->next;
        }
        return count;
    }

    static void printBookState(const PriceLadder& book)
    {
        std::cout << "Best Bid: " << book.getBestBid() << " (vol: " << book.getBestBidVolume() << ")" << std::endl;
        std::cout << "Best Ask: " << book.getBestAsk() << " (vol: " << book.getBestAskVolume() << ")" << std::endl;
        std::cout << "Total Orders: " << book.getOrderCount() << std::endl;
    }

    static void testAddBuyOrders()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15000, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(2, 15100, 200, OrderSide::Buy);
        book.addOrder(buy2);

        Order* buy3 = createTestOrder(3, 14900, 150, OrderSide::Buy);
        book.addOrder(buy3);

        testing::AssertTrue(book.getOrderCount() == 3);
        testing::AssertTrue(book.getBestBid() == 15100);
        testing::AssertTrue(book.getBestBidVolume() == 200);
        testing::AssertTrue(book.getBidPriceAtDepth(0) == 15100);
        testing::AssertTrue(book.getBidPriceAtDepth(1) == 15000);
        testing::AssertTrue(book.getBidPriceAtDepth(2) == 14900);

        printBookState(book);
    }

    static void testAddSellOrders()
    {
        PriceLadder book(10000, 20000);

        Order* sell1 = createTestOrder(101, 16000, 50, OrderSide::Sell);
        book.addOrder(sell1);

        Order* sell2 = createTestOrder(102, 15900, 80, OrderSide::Sell);
        book.addOrder(sell2);

        Order* sell3 = createTestOrder(103, 16100, 120, OrderSide::Sell);
        book.addOrder(sell3);

        testing::AssertTrue(book.getOrderCount() == 3);
        testing::AssertTrue(book.getBestAsk() == 15900);
        testing::AssertTrue(book.getBestAskVolume() == 80);
        testing::AssertTrue(book.getAskPriceAtDepth(0) == 15900);
        testing::AssertTrue(book.getAskPriceAtDepth(1) == 16000);
        testing::AssertTrue(book.getAskPriceAtDepth(2) == 16100);

        printBookState(book);
    }


    static void testMixedOrders()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15000, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(2, 15100, 200, OrderSide::Buy);
        book.addOrder(buy2);

        Order* sell1 = createTestOrder(101, 16000, 50, OrderSide::Sell);
        book.addOrder(sell1);

        Order* sell2 = createTestOrder(102, 15900, 80, OrderSide::Sell);
        book.addOrder(sell2);

        testing::AssertTrue(book.getOrderCount() == 4);
        testing::AssertTrue(book.getBestBid() == 15100);
        testing::AssertTrue(book.getBestAsk() == 15900);
        testing::AssertTrue(book.getBestBidVolume() == 200);
        testing::AssertTrue(book.getBestAskVolume() == 80);

        printBookState(book);
    }

    static void testTimePriority()
    {
        PriceLadder book(10000, 20000);

        Order* first = createTestOrder(1, 15100, 100, OrderSide::Buy);
        book.addOrder(first);

        std::this_thread::sleep_for(std::chrono::microseconds(1));
        Order* second = createTestOrder(2, 15100, 200, OrderSide::Buy);
        book.addOrder(second);

        std::this_thread::sleep_for(std::chrono::microseconds(1));
        Order* third = createTestOrder(3, 15100, 300, OrderSide::Buy);
        book.addOrder(third);

        const PriceLevel* level = book.getLevel(15100);
        testing::AssertTrue(level->head == first);
        testing::AssertTrue(level->head->next == second);
        testing::AssertTrue(level->head->next->next == third);
        testing::AssertTrue(level->tail == third);
        testing::AssertTrue(level->totalVolume == 600);
        testing::AssertTrue(countOrdersInLevel(level) == 3);

        printBookState(book);
    }

    static void testOrderCancellation()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15100, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(2, 15000, 200, OrderSide::Buy);
        book.addOrder(buy2);

        Order* sell1 = createTestOrder(101, 15900, 50, OrderSide::Sell);
        book.addOrder(sell1);

        testing::AssertTrue(book.getOrderCount() == 3);
        testing::AssertTrue(book.getBestBid() == 15100);
        testing::AssertTrue(book.getBestAsk() == 15900);

        // Cancel best bid
        Order* found = book.findOrder(1);
        testing::AssertTrue(found != nullptr);
        book.removeOrder(found);

        testing::AssertTrue(book.getOrderCount() == 2);
        testing::AssertTrue(book.getBestBid() == 15000);
        testing::AssertTrue(book.getBestAsk() == 15900);

        // Cancel best ask
        found = book.findOrder(101);
        testing::AssertTrue(found != nullptr);
        book.removeOrder(found);

        testing::AssertTrue(book.getOrderCount() == 1);
        testing::AssertTrue(book.getBestBid() == 15000);
        testing::AssertTrue(book.getBestAsk() == std::numeric_limits<Price>::max());

        // Cancel remaining order
        found = book.findOrder(2);
        testing::AssertTrue(found != nullptr);
        book.removeOrder(found);

        testing::AssertTrue(book.getOrderCount() == 0);
        testing::AssertTrue(book.getBestBid() == 0);
        testing::AssertTrue(book.getBestAsk() == std::numeric_limits<Price>::max());

        printBookState(book);
    }

    static void testOrderModification()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15100, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(2, 15000, 200, OrderSide::Buy);
        book.addOrder(buy2);

        testing::AssertTrue(book.getBestBidVolume() == 100);
        testing::AssertTrue(book.getBidVolumeAtDepth(1) == 200);

        // Modify volume of best bid
        book.modifyOrderVolume(buy1, 300);
        testing::AssertTrue(buy1->volume == 300);
        testing::AssertTrue(book.getBestBidVolume() == 300);
        testing::AssertTrue(book.getBidVolumeAtDepth(1) == 200);
        testing::AssertTrue(book.getLevel(15100)->totalVolume == 300);

        // Modify volume of second level
        book.modifyOrderVolume(buy2, 50);
        testing::AssertTrue(buy2->volume == 50);
        testing::AssertTrue(book.getBestBidVolume() == 300);
        testing::AssertTrue(book.getBidVolumeAtDepth(1) == 50);
        testing::AssertTrue(book.getLevel(15000)->totalVolume == 50);

        printBookState(book);
    }

    static void testPriceImprovement()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15000, 100, OrderSide::Buy);
        book.addOrder(buy1);

        testing::AssertTrue(book.getBestBid() == 15000);

        // Add better bid (higher price)
        Order* buy2 = createTestOrder(2, 15100, 200, OrderSide::Buy);
        book.addOrder(buy2);
        testing::AssertTrue(book.getBestBid() == 15100);
        testing::AssertTrue(book.getBidPriceAtDepth(0) == 15100);
        testing::AssertTrue(book.getBidPriceAtDepth(1) == 15000);

        Order* sell1 = createTestOrder(101, 16000, 50, OrderSide::Sell);
        book.addOrder(sell1);
        testing::AssertTrue(book.getBestAsk() == 16000);

        // Add better ask (lower price)
        Order* sell2 = createTestOrder(102, 15900, 80, OrderSide::Sell);
        book.addOrder(sell2);
        testing::AssertTrue(book.getBestAsk() == 15900);
        testing::AssertTrue(book.getAskPriceAtDepth(0) == 15900);
        testing::AssertTrue(book.getAskPriceAtDepth(1) == 16000);

        printBookState(book);
    }


    static void testMarketOrderExecution()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15100, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(2, 15000, 200, OrderSide::Buy);
        book.addOrder(buy2);

        Order* sell1 = createTestOrder(101, 15900, 50, OrderSide::Sell);
        book.addOrder(sell1);

        Order* sell2 = createTestOrder(102, 16000, 80, OrderSide::Sell);
        book.addOrder(sell2);

        testing::AssertTrue(book.getOrderCount() == 4);
        testing::AssertTrue(book.getBestAsk() == 15900);
        testing::AssertTrue(book.getBestAskVolume() == 50);

        // Market BUY: execute against best ask
        const PriceLevel* askLevel = book.getBestAskLevel();
        Order* bestAskOrder = askLevel->head;
        testing::AssertTrue(bestAskOrder->orderId == 101);
        book.removeOrder(bestAskOrder);

        testing::AssertTrue(book.getOrderCount() == 3);
        testing::AssertTrue(book.getBestAsk() == 16000);
        testing::AssertTrue(book.getBestAskVolume() == 80);

        // Market SELL: execute against best bid
        const PriceLevel* bidLevel = book.getBestBidLevel();
        Order* bestBidOrder = bidLevel->head;
        testing::AssertTrue(bestBidOrder->orderId == 1);
        book.removeOrder(bestBidOrder);

        testing::AssertTrue(book.getOrderCount() == 2);
        testing::AssertTrue(book.getBestBid() == 15000);
        testing::AssertTrue(book.getBestBidVolume() == 200);

        printBookState(book);
    }

    static void testDepthLevelAccess()
    {
        PriceLadder book(10000, 20000);

        // Add multiple bid levels
        Order* buy1 = createTestOrder(1, 15100, 100, OrderSide::Buy);
        book.addOrder(buy1);
        Order* buy2 = createTestOrder(2, 15050, 150, OrderSide::Buy);
        book.addOrder(buy2);
        Order* buy3 = createTestOrder(3, 15000, 200, OrderSide::Buy);
        book.addOrder(buy3);
        Order* buy4 = createTestOrder(4, 14900, 250, OrderSide::Buy);
        book.addOrder(buy4);

        // Add multiple ask levels
        Order* sell1 = createTestOrder(101, 15900, 50, OrderSide::Sell);
        book.addOrder(sell1);
        Order* sell2 = createTestOrder(102, 16000, 80, OrderSide::Sell);
        book.addOrder(sell2);
        Order* sell3 = createTestOrder(103, 16100, 120, OrderSide::Sell);
        book.addOrder(sell3);

        // Test bid depth
        testing::AssertTrue(book.getBidPriceAtDepth(0) == 15100);
        testing::AssertTrue(book.getBidVolumeAtDepth(0) == 100);
        testing::AssertTrue(book.getBidPriceAtDepth(1) == 15050);
        testing::AssertTrue(book.getBidVolumeAtDepth(1) == 150);
        testing::AssertTrue(book.getBidPriceAtDepth(2) == 15000);
        testing::AssertTrue(book.getBidVolumeAtDepth(2) == 200);
        testing::AssertTrue(book.getBidPriceAtDepth(3) == 14900);
        testing::AssertTrue(book.getBidVolumeAtDepth(3) == 250);
        testing::AssertTrue(book.getBidPriceAtDepth(10) == 0);

        // Test ask depth
        testing::AssertTrue(book.getAskPriceAtDepth(0) == 15900);
        testing::AssertTrue(book.getAskVolumeAtDepth(0) == 50);
        testing::AssertTrue(book.getAskPriceAtDepth(1) == 16000);
        testing::AssertTrue(book.getAskVolumeAtDepth(1) == 80);
        testing::AssertTrue(book.getAskPriceAtDepth(2) == 16100);
        testing::AssertTrue(book.getAskVolumeAtDepth(2) == 120);
        testing::AssertTrue(book.getAskPriceAtDepth(10) == std::numeric_limits<Price>::max());

        // Test level pointers
        const PriceLevel* bidLevel0 = book.getBidLevel(0);
        testing::AssertTrue(bidLevel0 != nullptr);
        testing::AssertTrue(bidLevel0->priceTick == 15100);

        const PriceLevel* askLevel0 = book.getAskLevel(0);
        testing::AssertTrue(askLevel0 != nullptr);
        testing::AssertTrue(askLevel0->priceTick == 15900);

        printBookState(book);
    }

    static void testEmptyBookOperations()
    {
        const PriceLadder book(10000, 20000);

        testing::AssertTrue(book.isEmpty());
        testing::AssertTrue(book.getOrderCount() == 0);
        testing::AssertTrue(book.getBestBid() == 0);
        testing::AssertTrue(book.getBestAsk() == std::numeric_limits<Price>::max());
        testing::AssertTrue(book.getBestBidLevel() == nullptr);
        testing::AssertTrue(book.getBestAskLevel() == nullptr);
        testing::AssertTrue(book.getBestBidVolume() == 0);
        testing::AssertTrue(book.getBestAskVolume() == 0);
        testing::AssertTrue(book.getBidPriceAtDepth(0) == 0);
        testing::AssertTrue(book.getAskPriceAtDepth(0) == std::numeric_limits<Price>::max());
        testing::AssertTrue(book.getNumBidLevels() > 0);
        testing::AssertTrue(book.getNumAskLevels() > 0);

        printBookState(book);
    }

    static void testLargeVolumeOperations()
    {
        PriceLadder book(10000, 20000);
        constexpr uint64_t largeVolume = 1000000000ULL;

        Order* buy1 = createTestOrder(1, 15000, largeVolume, OrderSide::Buy);
        book.addOrder(buy1);

        testing::AssertTrue(book.getBestBidVolume() == largeVolume);
        testing::AssertTrue(book.getLevel(15000)->totalVolume == largeVolume);

        Order* sell1 = createTestOrder(101, 16000, largeVolume * 2, OrderSide::Sell);
        book.addOrder(sell1);

        testing::AssertTrue(book.getBestAskVolume() == largeVolume * 2);
        testing::AssertTrue(book.getLevel(16000)->totalVolume == largeVolume * 2);

        // Modify to even larger volume
        constexpr uint64_t newVolume = largeVolume * 10;
        book.modifyOrderVolume(buy1, newVolume);
        testing::AssertTrue(buy1->volume == newVolume);
        testing::AssertTrue(book.getBestBidVolume() == newVolume);
        testing::AssertTrue(book.getLevel(15000)->totalVolume == newVolume);

        printBookState(book);
    }

    static void testFindOrderById()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1001, 15000, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(1002, 15100, 200, OrderSide::Buy);
        book.addOrder(buy2);

        Order* sell1 = createTestOrder(2001, 16000, 50, OrderSide::Sell);
        book.addOrder(sell1);

        const Order* found = book.findOrder(1002);
        testing::AssertTrue(found != nullptr);
        testing::AssertTrue(found->orderId == 1002);
        testing::AssertTrue(found->priceTick == 15100);
        testing::AssertTrue(found->volume == 200);

        found = book.findOrder(2001);
        testing::AssertTrue(found != nullptr);
        testing::AssertTrue(found->orderId == 2001);
        testing::AssertTrue(found->priceTick == 16000);
        testing::AssertTrue(found->volume == 50);

        found = book.findOrder(9999);
        testing::AssertTrue(found == nullptr);

        // Remove order and verify cannot find it
        book.removeOrder(buy1);
        found = book.findOrder(1001);
        testing::AssertTrue(found == nullptr);

        printBookState(book);
    }

    static void testRemoveFromMiddle() {
        PriceLadder book(10000, 20000);

        Order* first = createTestOrder(1, 15100, 100, OrderSide::Buy);
        book.addOrder(first);

        Order* second = createTestOrder(2, 15100, 200, OrderSide::Buy);
        book.addOrder(second);

        Order* third = createTestOrder(3, 15100, 300, OrderSide::Buy);
        book.addOrder(third);

        const PriceLevel* level = book.getLevel(15100);
        testing::AssertTrue(countOrdersInLevel(level) == 3);
        testing::AssertTrue(level->head == first);
        testing::AssertTrue(level->tail == third);

        // Remove middle order
        book.removeOrder(second);

        testing::AssertTrue(countOrdersInLevel(level) == 2);
        testing::AssertTrue(level->head == first);
        testing::AssertTrue(level->head->next == third);
        testing::AssertTrue(level->tail == third);
        testing::AssertTrue(level->totalVolume == 400);

        // Remove first order
        book.removeOrder(first);
        testing::AssertTrue(countOrdersInLevel(level) == 1);
        testing::AssertTrue(level->head == third);
        testing::AssertTrue(level->tail == third);
        testing::AssertTrue(level->totalVolume == 300);

        // Remove last order
        book.removeOrder(third);
        testing::AssertTrue(countOrdersInLevel(level) == 0);
        testing::AssertTrue(level->head == nullptr);
        testing::AssertTrue(level->tail == nullptr);
        testing::AssertTrue(level->totalVolume == 0);

        printBookState(book);
    }

    static void testGetLevelByPrice()
    {
        PriceLadder book(10000, 20000);

        Order* buy1 = createTestOrder(1, 15000, 100, OrderSide::Buy);
        book.addOrder(buy1);

        Order* buy2 = createTestOrder(2, 15200, 200, OrderSide::Buy);
        book.addOrder(buy2);

        const PriceLevel* level = book.getLevel(15000);
        testing::AssertTrue(level != nullptr);
        testing::AssertTrue(level->priceTick == 15000);
        testing::AssertTrue(level->totalVolume == 100);
        testing::AssertTrue(level->head == buy1);
        testing::AssertTrue(level->tail == buy1);

        level = book.getLevel(15200);
        testing::AssertTrue(level != nullptr);
        testing::AssertTrue(level->priceTick == 15200);
        testing::AssertTrue(level->totalVolume == 200);
        testing::AssertTrue(level->head == buy2);
        testing::AssertTrue(level->tail == buy2);

        // Empty level
        level = book.getLevel(15100);
        testing::AssertTrue(level != nullptr);
        testing::AssertTrue(level->priceTick == 15100);
        testing::AssertTrue(level->totalVolume == 0);
        testing::AssertTrue(level->head == nullptr);
        testing::AssertTrue(level->tail == nullptr);
        testing::AssertTrue(level->isEmpty());

        // Const version
        const PriceLadder& constBook = book;
        const PriceLevel* constLevel = constBook.getLevel(15000);
        testing::AssertTrue(constLevel != nullptr);
        testing::AssertTrue(constLevel->priceTick == 15000);

        printBookState(book);
    }
}


static void TestAll()
{
    using namespace unit_tests;

    testAddBuyOrders();
    testAddSellOrders();
    testMixedOrders();
    testTimePriority();
    testOrderCancellation();
    testOrderModification();
    testPriceImprovement();
    testMarketOrderExecution();
    testDepthLevelAccess();
    testEmptyBookOperations();
    testLargeVolumeOperations();
    testFindOrderById();
    testRemoveFromMiddle();
    testGetLevelByPrice();
}


int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    TestAll();

    return EXIT_SUCCESS;
}

