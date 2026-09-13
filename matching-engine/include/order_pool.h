#pragma once

#include <vector>
#include "order.h"

//pre-allocate memory for orders to avoid frequent allocations
inline std::vector<Order> order_pool = [] {
    std::vector<Order> v;
    v.resize(1000000); // allocate 1 million default-constructed orders
    return v;
}();

//free list to keep track of available orders in the pool
inline std::vector<Order*> free_list;

//initialize the free list with all orders in the pool
inline void initialize_free_list() {
    free_list.clear();
    free_list.reserve(order_pool.size());
    for (auto& order : order_pool) {
        free_list.push_back(&order);
    }
}
//acquire an order from the free list
inline Order* acquire_order() {
    if (free_list.empty()) {
        return nullptr; // no available orders
    }
    Order* order = free_list.back();
    free_list.pop_back();
    return order;
}

//release an order back to the free list
inline void release_order(Order* order) {
    free_list.push_back(order);
}

