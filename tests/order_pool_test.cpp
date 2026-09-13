#include "order_pool.h"
#include "gtest/gtest.h"

TEST(OrderPoolTest, AcquireAndReleaseOrder) {
    initialize_free_list();
    Order* order = acquire_order();
    ASSERT_NE(order, nullptr); // should acquire an order successfully
    release_order(order);
    ASSERT_EQ(free_list.size(), order_pool.size()); // all orders should be back in the free list
}

TEST(OrderPoolTest, AcquireAllOrders) {
    initialize_free_list();
    std::vector<Order*> acquired_orders;
    while (Order* order = acquire_order()) {
        acquired_orders.push_back(order);
    }
    ASSERT_EQ(acquired_orders.size(), order_pool.size()); // should acquire all orders
    for (Order* order : acquired_orders) {
        release_order(order);
    }
    ASSERT_EQ(free_list.size(), order_pool.size()); // all orders should be back in the free list
}

TEST(OrderPoolTest, AcquireOrderWhenPoolIsEmpty) {
    initialize_free_list();
    // acquire all orders to empty the pool
    while (acquire_order()) {}
    // now the pool is empty, acquiring an order should return nullptr
    Order* order = acquire_order();
    ASSERT_EQ(order, nullptr); // should not acquire any order
}

TEST(OrderPoolTest, ReleaseOrderBackToPool) {
    initialize_free_list();
    Order* order = acquire_order();
    ASSERT_NE(order, nullptr); // should acquire an order successfully
    release_order(order);
    ASSERT_EQ(free_list.size(), order_pool.size()); // all orders should be back in the free list
}
