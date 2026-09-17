/**============================================================================
Name        : PriceLadder.cpp
Created on  : 17.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : PriceLadder.cpp
============================================================================**/

#include "PriceLadder.hpp"


namespace price_ladder
{
    PriceLadder::PriceLadder(const Price minPriceTick, const Price maxPriceTick) noexcept:
          minPriceTick(minPriceTick), maxPriceTick(maxPriceTick), numLevels(maxPriceTick - minPriceTick + 1)
    {
        levels.reserve(numLevels);
        for (Price price = minPriceTick; price <= maxPriceTick; ++price) {
            levels.emplace_back(price);
        }
        orderIndex.reserve(kDefaultReserveSize);
    }

    PriceLevel* PriceLadder::getLevel(const Price priceTick) noexcept
    {
        const size_t index = priceTick - minPriceTick;
        return &levels[index];
    }

    const PriceLevel* PriceLadder::getLevel(const Price priceTick) const noexcept
    {
        const size_t index = priceTick - minPriceTick;
        return &levels[index];
    }

    void PriceLadder::addOrder(Order* order) noexcept
    {
        PriceLevel* level = getLevel(order->priceTick);
        level->addOrder(order);
        orderIndex.emplace(order->orderId, order);
    }

    void PriceLadder::removeOrder(Order* order) noexcept
    {
        order->level->removeOrder(order);
        orderIndex.erase(order->orderId);
    }

    void PriceLadder::modifyOrderVolume(Order* order, const Volume newVolume) noexcept
    {
        order->level->totalVolume -= order->volume;
        order->level->totalVolume += newVolume;
        order->volume = newVolume;
    }

    Order* PriceLadder::findOrder(const OrderId orderId) const noexcept {
        const auto it = orderIndex.find(orderId);
        return (it != orderIndex.end()) ? it->second : nullptr;
    }

    OrderId PriceLadder::getBestBid() const noexcept
    {
        for (int64_t i = static_cast<int64_t>(numLevels) - 1; i >= 0; --i) {
            if (levels[i].head != nullptr) {
                return levels[i].priceTick;
            }
        }
        return 0;
    }

    [[nodiscard]]
    Price PriceLadder::getBestAsk() const noexcept
    {
        for (size_t i = 0; i < numLevels; ++i) {
            if (levels[i].head != nullptr) {
                return levels[i].priceTick;
            }
        }
        return UINT64_MAX;
    }

    bool PriceLadder::isEmpty() const noexcept {
        return orderIndex.empty();
    }

    size_t PriceLadder::getOrderCount() const noexcept {
        return orderIndex.size();
    }

    Price PriceLadder::getMinPriceTick() const noexcept {
        return minPriceTick;
    }

    Price PriceLadder::getMaxPriceTick() const noexcept {
        return maxPriceTick;
    }

    size_t PriceLadder::getNumLevels() const noexcept {
        return numLevels;
    }
}