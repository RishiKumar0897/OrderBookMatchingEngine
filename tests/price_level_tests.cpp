#include "price_level.h"
#include "gtest/gtest.h"

TEST(PriceLevelTest, PushBackAndFront) {
    PriceLevel level(100);
    Order order1;
    order1.remaining_qty = 10;
    Order order2;
    order2.remaining_qty = 20;

    level.push_back(&order1);
    level.push_back(&order2);

    ASSERT_EQ(level.front(), &order1);
    ASSERT_EQ(level.back(), &order2);
    ASSERT_EQ(level.total_qty(), 30);
}

TEST(PriceLevelTest, RemoveOrder) {
    PriceLevel level(100);
    Order order1;
    order1.remaining_qty = 10;
    Order order2;
    order2.remaining_qty = 20;

    level.push_back(&order1);
    level.push_back(&order2);

    level.remove(&order1);
    ASSERT_EQ(level.front(), &order2);
    ASSERT_EQ(level.back(), &order2);
    ASSERT_EQ(level.total_qty(), 20);

    level.remove(&order2);
    ASSERT_TRUE(level.empty());
    ASSERT_EQ(level.total_qty(), 0);
}

TEST(PriceLevelTest, RemoveMiddleOrder) {
    PriceLevel level(100);
    Order order1;
    order1.remaining_qty = 10;
    Order order2;
    order2.remaining_qty = 20;
    Order order3;
    order3.remaining_qty = 30;

    level.push_back(&order1);
    level.push_back(&order2);
    level.push_back(&order3);

    level.remove(&order2);
    ASSERT_EQ(level.front(), &order1);
    ASSERT_EQ(level.back(), &order3);
    ASSERT_EQ(level.total_qty(), 40);
}

TEST(PriceLevelTest, RemoveHeadAndTail) {
    PriceLevel level(100);
    Order order1;
    order1.remaining_qty = 10;
    Order order2;
    order2.remaining_qty = 20;

    level.push_back(&order1);
    level.push_back(&order2);

    level.remove(&order1); // remove head
    ASSERT_EQ(level.front(), &order2);
    ASSERT_EQ(level.back(), &order2);
    ASSERT_EQ(level.total_qty(), 20);

    level.remove(&order2); // remove tail
    ASSERT_TRUE(level.empty());
    ASSERT_EQ(level.total_qty(), 0);
}
