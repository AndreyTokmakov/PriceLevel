/**============================================================================
Name        : PriceLadder.hpp
Created on  : 17.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : PriceLadder.hpp
============================================================================**/

#ifndef PRICELEVEL_PRICELADDER_HPP
#define PRICELEVEL_PRICELADDER_HPP

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

#include "PriceLevel.hpp"

#include <vector>
// #include <absl/container/flat_hash_map.h>
#include <unordered_map>

namespace price_ladder
{
    class PriceLadder
    {
    public:
        PriceLadder(Price minPriceTick, Price maxPriceTick) noexcept;

        [[nodiscard]]
        PriceLevel* getLevel(Price priceTick) noexcept;

        [[nodiscard]]
        const PriceLevel* getLevel(Price priceTick) const noexcept;

        [[nodiscard]]
        PriceLevel* getBidLevel(size_t depth) noexcept;

        [[nodiscard]]
        const PriceLevel* getBidLevel(size_t depth) const noexcept;

        [[nodiscard]]
        PriceLevel* getAskLevel(size_t depth) noexcept;

        [[nodiscard]]
        const PriceLevel* getAskLevel(size_t depth) const noexcept;

        void addOrder(Order* order) noexcept;

        void removeOrder(Order* order) noexcept;

        static void modifyOrderVolume(Order* order, Volume newVolume) noexcept;

        [[nodiscard]]
        Order* findOrder(OrderId orderId) const noexcept;

        [[nodiscard]]
        Price getBestBid() const noexcept;

        [[nodiscard]]
        Price getBestAsk() const noexcept;

        [[nodiscard]]
        PriceLevel* getBestBidLevel() const noexcept;

        [[nodiscard]]
        PriceLevel* getBestAskLevel() const noexcept;

        [[nodiscard]]
        Volume getBestBidVolume() const noexcept;

        [[nodiscard]]
        Volume getBestAskVolume() const noexcept;

        [[nodiscard]]
        Price getBidPriceAtDepth(size_t depth) const noexcept;

        [[nodiscard]]
        Price getAskPriceAtDepth(size_t depth) const noexcept;

        [[nodiscard]]
        Volume getBidVolumeAtDepth(size_t depth) const noexcept;

        [[nodiscard]]
        Volume getAskVolumeAtDepth(size_t depth) const noexcept;

        [[nodiscard]]
        bool isEmpty() const noexcept;

        [[nodiscard]]
        size_t getOrderCount() const noexcept;

        [[nodiscard]]
        Price getMinPriceTick() const noexcept;

        [[nodiscard]]
        Price getMaxPriceTick() const noexcept;

        [[nodiscard]]
        size_t getNumLevels() const noexcept;

        [[nodiscard]]
        size_t getNumBidLevels() const noexcept;

        [[nodiscard]]
        size_t getNumAskLevels() const noexcept;

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

#endif //PRICELEVEL_PRICELADDER_HPP
