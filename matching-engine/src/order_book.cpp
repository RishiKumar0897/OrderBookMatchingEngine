#include <vector>
#include "order_book.h"


OrderBook::OrderBook(int symbol_id) : symbol_id_(symbol_id) {}


std::vector<Trade> OrderBook::add_limit_order(uint64_t order_id, OrderSide side,
            int64_t price, int64_t qty, uint64_t timestamp) {
    std::vector<Trade> trades;
    Order* order = acquire_order();
    if (!order) {
        // Handle the case where no orders are available in the pool
        return trades; // return empty trades
    }
    order->order_id = order_id;
    order->symbol_id = symbol_id_;
    order->side = side;
    order->price = price;
    order->qty = qty;
    order->remaining_qty = qty;
    order->timestamp = timestamp;
    order->next = nullptr;
    order->prev = nullptr;

    match(order, trades);

    if (order->remaining_qty > 0) {
        //rest it; find or create the price level for this order's price in the right side's map
        std::map<int64_t, PriceLevel, std::greater<int64_t>>& side_map = (side == OrderSide::BUY) ? bids_ : asks_;
        auto it = side_map.find(price);
    } else {
        //fully filled, release it back to the pool
        release_order(order);
    }
    return trades;
}

void OrderBook::match(Order* incoming_order, std::vector<Trade>& trades) {
    if (incoming_order->side == OrderSide::BUY) {
        while (incoming_order->remaining_qty > 0 && !asks_.empty()) {
            auto it = asks_.begin();
            PriceLevel& best_ask_level = it->second;
            if(best_ask_level.price() > incoming_order->price) {
                break; // no more crossing prices
            }

            Order* resting_order = best_ask_level.front();
            int64_t trade_qty = std::min(incoming_order->remaining_qty, resting_order->remaining_qty);
            trades.push_back({resting_order->order_id, incoming_order->order_id, best_ask_level.price(), trade_qty, incoming_order->timestamp});
            incoming_order->remaining_qty -= trade_qty;
            resting_order->remaining_qty -= trade_qty;

            if (resting_order->remaining_qty == 0) {
                best_ask_level.remove(resting_order);
                orders_by_id_.erase(resting_order->order_id);
                release_order(resting_order);
                if (best_ask_level.empty()) {
                    asks_.erase(it);
                }
            }
        }

    }
    else { // incoming_order->side == OrderSide::SELL
        while (incoming_order->remaining_qty > 0 && !bids_.empty()) {
            auto it = bids_.begin();
            PriceLevel& best_bid_level = it->second;
            if(best_bid_level.price() < incoming_order->price) {
                break; // no more crossing prices
            }

            Order* resting_order = best_bid_level.front();
            int64_t trade_qty = std::min(incoming_order->remaining_qty, resting_order->remaining_qty);
            trades.push_back({resting_order->order_id, incoming_order->order_id, best_bid_level.price(), trade_qty, incoming_order->timestamp});
            incoming_order->remaining_qty -= trade_qty;
            resting_order->remaining_qty -= trade_qty;

            if (resting_order->remaining_qty == 0) {
                best_bid_level.remove(resting_order);
                orders_by_id_.erase(resting_order->order_id);
                release_order(resting_order);
                if (best_bid_level.empty()) {
                    bids_.erase(it);
                }
            }
        }
    }
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        return false; // order not found
    }

    Order* order = it->second;
    if (!order) {
        return false; // order not found
    }
    std::map<int64_t, PriceLevel, std::greater<int64_t>>& side_map = (order->side == OrderSide::BUY) ? bids_ : asks_;
    auto level_it = side_map.find(order->price);
    if (level_it != side_map.end()) {
        PriceLevel& level = level_it->second;
        level.remove(order);
        if (level.empty()) {
            side_map.erase(level_it);
        }
    }

    orders_by_id_.erase(it);
    release_order(order);
    return true;
}

bool OrderBook::best_bid(int64_t& price_out) const {
    if (bids_.empty()) {
        return false;
    }
    price_out = bids_.begin()->first;
    return true;
}

bool OrderBook::best_ask(int64_t& price_out) const {
    if (asks_.empty()) {
        return false;
    }
    price_out = asks_.begin()->first;
    return true;
}

