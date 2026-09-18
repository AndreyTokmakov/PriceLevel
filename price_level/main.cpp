/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description :
============================================================================**/

#include <iostream>
#include <print>
#include <vector>
#include <string_view>
#include <filesystem>
#include <fstream>

#include "DateTimeUtilities.hpp"
#include "OrderPool.hpp"

namespace price_level
{


    class IntrusivePriceLevel
    {
    public:
        using Order = common::Order;
        using OrderId = Order::OrderId;
        using Price = Order::Price;
        using Quantity = Order::Quantity;

        explicit IntrusivePriceLevel(const Price price) noexcept : price(price) {}

        IntrusivePriceLevel(const IntrusivePriceLevel&) = delete;
        IntrusivePriceLevel& operator=(const IntrusivePriceLevel&) = delete;

        IntrusivePriceLevel(IntrusivePriceLevel&&) = delete;
        IntrusivePriceLevel& operator=(IntrusivePriceLevel&&) = delete;

        [[nodiscard]]
        Price getPrice() const noexcept {
            return price;
        }

        [[nodiscard]]
        Order* front() noexcept {
            return head;
        }

        [[nodiscard]]
        const Order* front() const noexcept {
            return head;
        }

        [[nodiscard]]
        Order* back() noexcept {
            return tail;
        }

        [[nodiscard]]
        const Order* back() const noexcept {
            return tail;
        }

        [[nodiscard]]
        Quantity total_quantity() const noexcept {
            return totalQuantity;
        }

        [[nodiscard]]
        std::size_t order_count() const noexcept {
            return orderCount;
        }

        [[nodiscard]]
        bool empty() const noexcept {
            return head == nullptr;
        }

        void pushBack(Order& order)
        {
            validateForInsert(order);

            order.prev = tail;
            order.next = nullptr;

            if (tail != nullptr) {
                tail->next = &order;
            } else {
                head = &order;
            }

            tail = &order;

            totalQuantity += order.quantity;
            ++orderCount;
        }

        [[nodiscard]]
        Order* popFront() noexcept
        {
            if (head == nullptr) {
                return nullptr;
            }

            Order* order = head;
            unlink(order);

            return order;
        }

        void remove(Order& order) {
            validateForRemove(order);
            unlink(&order);
        }

        void reduce(Order& order, const Quantity executed)
        {
            validateForRemove(order);

            if (executed <= 0 || executed > order.quantity) {
                throw std::invalid_argument("invalid executed quantity");
            }

            order.quantity -= executed;
            totalQuantity -= executed;

            if (order.quantity == 0) {
                unlink(&order);
            }
        }

    private:

        void validateForInsert
        (
            const Order& order) const {
            if (order.price != price) {
                throw std::invalid_argument("order price does not match price level");
            }
            if (order.quantity <= 0) {
                throw std::invalid_argument( "order quantity must be positive");
            }

            if (order.prev != nullptr || order.next != nullptr || head == &order || tail == &order) {
                throw std::logic_error("order is already linked");
            }
        }

        void validateForRemove(const Order& order) const
        {
            const bool is_single = head == &order && tail == &order && order.prev == nullptr && order.next == nullptr;
            const bool is_head = head == &order && order.prev == nullptr;
            const bool is_tail = tail == &order && order.next == nullptr;
            const bool is_middle = order.prev != nullptr && order.next != nullptr;

            if (!(is_single || is_head || is_tail || is_middle)) {
                throw std::logic_error( "order does not belong to this price level");
            }
        }

        void unlink(Order* order) noexcept
        {
            Order* previous = order->prev;
            Order* next = order->next;

            if (previous != nullptr) {
                previous->next = next;
            } else {
                head = next;
            }

            if (next != nullptr) {
                next->prev = previous;
            } else {
                tail = previous;
            }

            totalQuantity -= order->quantity;
            --orderCount;

            order->prev = nullptr;
            order->next = nullptr;
        }

    private:

        Price price;

        Order* head = nullptr;
        Order* tail = nullptr;

        Quantity totalQuantity = 0;
        std::size_t orderCount = 0;
    };

    void demo()
    {
        using Order = IntrusivePriceLevel::Order;

        // order_pool::ReusableOrderPool<1024> pool;
        order_pool::OrderPool pool(1024);

        IntrusivePriceLevel level(10025);

        Order* order1 = pool.create(1001, 10025, 10);
        Order* order2 = pool.create(1002, 10025, 20);
        Order* order3 = pool.create(1003, 10025, 30);

        level.pushBack(*order1);
        level.pushBack(*order2);
        level.pushBack(*order3);

        std::cout << "FIFO: ";

        for (const Order* order = level.front();
            order != nullptr;
            order = order->next) {
            std::cout << order->id << ' ';
        }

        std::cout << '\n';

        std::cout << "order count: " << level.order_count() << '\n';
        std::cout << "total quantity: " << level.total_quantity() << '\n';

        // Частичное исполнение первой заявки.
        level.reduce(*order1, 4);

        std::cout << "after partial fill:\n";
        std::cout << "order1 quantity: " << order1->quantity << '\n';
        std::cout << "level quantity: " << level.total_quantity() << '\n';

        // Удаление средней заявки.
        level.remove(*order2);
        pool.destroy(order2);

        // Удаление первой заявки.
        level.remove(*order1);
        pool.destroy(order1);

        // Удаление последней заявки.
        Order* remaining = level.popFront();

        if (remaining != nullptr) {
            pool.destroy(remaining);
        }

        std::cout << "final pool size: " << pool.size() << '\n';
        std::cout << "final level order count: " << level.order_count() << '\n';
    }
}


int main([[maybe_unused]] int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    price_level::demo();


    return EXIT_SUCCESS;
}
