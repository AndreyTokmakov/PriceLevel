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

    PriceLevel* PriceLadder::getBidLevel(const size_t depth) noexcept
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

    const PriceLevel* PriceLadder::getBidLevel(const size_t depth) const noexcept
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

    PriceLevel* PriceLadder::getAskLevel(const size_t depth) noexcept
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

    const PriceLevel* PriceLadder::getAskLevel(const size_t depth) const noexcept
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

    Order* PriceLadder::findOrder(const OrderId orderId) const noexcept
    {
        const auto it = orderIndex.find(orderId);
        return (it != orderIndex.end()) ? it->second : nullptr;
    }

    Price PriceLadder::getBestBid() const noexcept
    {
        for (size_t i = 0; i < bidLevels.size(); ++i) {
            if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                return bidLevels[i]->priceTick;
            }
        }
        return 0;
    }

    Price PriceLadder::getBestAsk() const noexcept
    {
        for (size_t i = 0; i < askLevels.size(); ++i) {
            if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                return askLevels[i]->priceTick;
            }
        }
        return std::numeric_limits<Price>::max();
    }

    PriceLevel* PriceLadder::getBestBidLevel() const noexcept
    {
        for (size_t i = 0; i < bidLevels.size(); ++i) {
            if (bidLevels[i] && bidLevels[i]->hasBuyOrders()) {
                return bidLevels[i];
            }
        }
        return nullptr;
    }

    PriceLevel* PriceLadder::getBestAskLevel() const noexcept
    {
        for (size_t i = 0; i < askLevels.size(); ++i) {
            if (askLevels[i] && askLevels[i]->hasSellOrders()) {
                return askLevels[i];
            }
        }
        return nullptr;
    }

    Volume PriceLadder::getBestBidVolume() const noexcept
    {
        const PriceLevel* level = getBestBidLevel();
        return level ? level->getBuyVolume() : 0;
    }

    Volume PriceLadder::getBestAskVolume() const noexcept
    {
        const PriceLevel* level = getBestAskLevel();
        return level ? level->getSellVolume() : 0;
    }

    Price PriceLadder::getBidPriceAtDepth(const size_t depth) const noexcept {

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

    Price PriceLadder::getAskPriceAtDepth(const size_t depth) const noexcept
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

    Volume PriceLadder::getBidVolumeAtDepth(const size_t depth) const noexcept
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

    Volume PriceLadder::getAskVolumeAtDepth(const size_t depth) const noexcept
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

    size_t PriceLadder::getNumBidLevels() const noexcept {
        return bidLevels.size();
    }

    size_t PriceLadder::getNumAskLevels() const noexcept {
        return askLevels.size();
    }
}