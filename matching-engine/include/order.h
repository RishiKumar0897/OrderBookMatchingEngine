/*
 * Order.h
 * This file contains the definition of the Order struct and the OrderSide enum.
 */

//needed to avoid multiple inclusion of this header file
#pragma once

#include <cstdint>

enum class OrderSide {
    BUY,
    SELL
};

struct Order {
    uint64_t order_id;
    int symbol_id;
    OrderSide side;
    int64_t price;
    int64_t qty;
    int64_t remaining_qty;
    uint64_t timestamp;
    Order* next;
    Order* prev;

    Order() = default;
    Order(uint64_t id, int symbol, OrderSide s, int64_t p, int64_t q, uint64_t ts)
        : order_id(id), symbol_id(symbol), side(s), price(p), qty(q),
          remaining_qty(q), timestamp(ts), next(nullptr), prev(nullptr) {}
};
