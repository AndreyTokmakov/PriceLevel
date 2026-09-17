/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : 
============================================================================**/

#include <iostream>
#include <thread>

#include "Testing.hpp"
#include "PriceLadder.hpp"


namespace unit_tests
{
    using namespace price_ladder;

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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

        // printBookState(book);
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

