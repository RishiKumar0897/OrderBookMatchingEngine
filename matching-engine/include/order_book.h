#pragma once

#include <vector>
#include <cstdint>
#include <map>
#include <unordered_map>
#include "order.h"
#include "price_level.h"
#include "order_pool.h"

// Result of a single match between two orders, produced while matching an
// incoming ("taker") order against resting orders on the book ("makers").
// One incoming order can generate multiple Trades if it walks through
// several price levels / resting orders before it is fully filled.
struct Trade {
    uint64_t maker_order_id; // ID of the resting order that was matched against
    uint64_t taker_order_id; // ID of the incoming order that initiated the match
    int64_t price; // price at which the trade occurred (fixed-point, matches Order::price)
    int64_t qty; // quantity of the trade (fixed-point, matches Order::remaining_qty)
    uint64_t timestamp; // timestamp of the trade (fixed-point, matches Order::timestamp)
};

// A single-symbol limit order book: maintains resting buy (bid) and sell
// (ask) orders on either side of the market, matches incoming orders against
// them price-time priority style, and reports the resulting trades.
//
// Orders are stored per price in a PriceLevel (FIFO queue), and price levels
// are stored in maps kept sorted so the best bid/ask is always the first
// entry. Order lifetime (allocation) is owned by the order_pool, not by
// OrderBook — this class only holds raw Order* pointers into that pool.
class OrderBook {
    public:
        explicit OrderBook(int symbol_id);

        // Submits a new limit order: first tries to match it immediately
        // against the opposite side of the book, then rests whatever
        // quantity remains at its price level. Returns every Trade produced
        // by the match (empty if nothing crossed).
        std::vector<Trade> add_limit_order(uint64_t order_id, OrderSide side,
            int64_t price, int64_t qty, uint64_t timestamp);

        // Cancels a still-resting order by ID, removing it from its price
        // level. Returns false if no such order is currently on the book
        // (e.g. already filled or already cancelled).
        bool cancel_order(uint64_t order_id);

        // Best (highest) resting bid / best (lowest) resting ask price.
        // Returns false and leaves price_out untouched if that side is empty.
        bool best_bid(int64_t& price_out) const;
        bool best_ask(int64_t& price_out) const;

    private:
        // Crosses incoming_order against the opposite side of the book,
        // consuming resting orders (best price first, then FIFO within a
        // price level) until either incoming_order is fully filled or no
        // more opposing orders cross its price. Appends a Trade per fill
        // and mutates/removes fully-filled resting orders as it goes.
        void match(Order* incoming_order, std::vector<Trade>& trades);

        int symbol_id_; // which instrument this book is for

        // Resting orders grouped by price level. Bids are sorted descending
        // (highest price = best, matched first); asks ascending (lowest
        // price = best). std::map keeps levels ordered so begin() is always
        // the best price on that side.
        std::map<int64_t, PriceLevel, std::greater<int64_t>> bids_; // price -> PriceLevel, sorted descending
        std::map<int64_t, PriceLevel, std::less<int64_t>> asks_; // price -> PriceLevel, sorted ascending

        std::unordered_map<uint64_t, Order*> orders_by_id_; // order_id -> Order*, for quick lookup during cancels
};
