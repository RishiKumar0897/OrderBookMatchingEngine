#pragma once

#include "order.h"

// Represents a price level in the order book.
// Orders are held in an intrusive doubly-linked list (via Order::next/prev)
// so adding/removing an order never allocates.
class PriceLevel {
public:
    explicit PriceLevel(int64_t price) : price_(price), total_qty_(0), head_(nullptr), tail_(nullptr) {}

    int64_t price() const { return price_; }
    int64_t total_qty() const { return total_qty_; }

    // Enqueue at the tail, i.e. behind all resting orders at this price —
    // this is what gives the level FIFO (price-time) priority.
    void push_back(Order* order) {
        total_qty_ += order->remaining_qty;
        order->next = nullptr;
        order->prev = tail_;
        if (tail_) {
            tail_->next = order;
        } else {
            head_ = order;
        }
        tail_ = order;
    }

    // Unlink order from wherever it sits in the list (head, tail, or middle)
    // and patch up its neighbors. Used both for cancels and for orders that
    // are fully filled during matching.
    void remove(Order* order) {
        total_qty_ -= order->remaining_qty;

        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head_ = order->next;
        }

        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail_ = order->prev;
        }

        order->next = nullptr;
        order->prev = nullptr;
    }

    void reduce_qty(int64_t amount) { total_qty_ -= amount; }

    // front() is the next order to match against at this price (oldest / highest priority).
    Order* front() const { return head_; }
    Order* back() const { return tail_; }

    bool empty() const { return head_ == nullptr; }

private:
    int64_t price_;     // price this level represents (fixed-point, matches Order::price)
    int64_t total_qty_; // sum of remaining_qty across all resting orders, kept incrementally
    Order* head_;       // oldest order at this price (matched first)
    Order* tail_;       // newest order at this price (matched last)
};
