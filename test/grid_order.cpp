
#include <gtest/gtest.h>
#include <roq/order_status.h>
#include <roq/order_update.h>
#include <iostream>
#include "roq/shared/grid_order.h"
#include "roq/format.h"
#include "roq/utils/compare.h"
#include "roq/shared/grid_order.inl"
using namespace roq;
using namespace roq::utils;

struct MockStrategy {
  order_txid_t create_order(order_txid_t id, const Order& order) {
      create_count++;
      fmt::print(stderr, "create_order ( id: {} {}, price: {}, side: {} )\n"
        , id.order_id, id.client_order_id, order.price(), order.side());
      return id;
  }

  order_txid_t cancel_order(order_txid_t id) {
      cancel_count++;
      fmt::print(stderr, "cancel_order ( id: {} {} )\n"
        , id.order_id, id.client_order_id);
      return id;
  }

  //! returns new client order id
  order_txid_t modify_order(order_txid_t id, const Order& order) {
      modify_count++;
      fmt::print(stderr, "modify_order ( id: {} {}, price: {}, side: {}, flags: {} )\n"
        , id.order_id, id.client_order_id, order.price(), order.side(), order.flags);      
      return id;
  }

  // new order, new tx
  order_txid_t next_order_txid() { 
    ++txid_.order_id;
    ++txid_.client_order_id;
    return txid_; 
  }
  
  // same order, new tx
  order_txid_t next_order_txid(order_id_t id) { 
    order_txid_t txid {id, ++txid_.client_order_id};
    return txid; 
  }

  order_txid_t txid_ {};
  int create_count = 0;
  int modify_count = 0;
  int cancel_count = 0;
  int total_count() { return create_count+modify_count+cancel_count; }
};

TEST(levels, case_insensitive) {
  Levels<Level, 1> levels (1.0);
  std::array<price_t, 3> prices = {101., 100., 99.};
  
  EXPECT_TRUE(levels.empty());

  EXPECT_EQ(levels[100.].price, 100.);
  EXPECT_TRUE(levels[100.].empty());
  levels[100.].desired_volume = 100.;
  EXPECT_EQ(levels[100.].desired_volume, 100.);
  EXPECT_FALSE(levels[100.].empty());
  EXPECT_EQ(levels.size(), 1);
  EXPECT_FALSE(levels.empty());
  
  EXPECT_EQ(levels[99.].price, 99.);
  EXPECT_TRUE(levels[99.].empty());
  levels[99.].desired_volume = 99.;
  EXPECT_EQ(levels[99.].desired_volume, 99.);
  EXPECT_FALSE(levels[99.].empty());
  EXPECT_EQ(levels.size(), 2);
  EXPECT_FALSE(levels.empty());

  EXPECT_EQ(levels[101.].price, 101.);
  EXPECT_TRUE(levels[101.].empty());
  levels[101.].working_volume = 101.;
  EXPECT_EQ(levels[101.].working_volume, 101.);
  EXPECT_FALSE(levels[101.].empty());
  EXPECT_EQ(levels.size(), 3);
  EXPECT_FALSE(levels.empty());

  std::size_t i=0;
  for(auto& level: levels) {
    EXPECT_EQ(level.price, prices[i]);
    i++;
  }

  levels.erase(100.);
  EXPECT_TRUE(levels[100.].empty());
  EXPECT_EQ(levels.size(), 3);
  EXPECT_EQ(levels.nth(0).price, 101.);
  EXPECT_EQ(levels.nth(1).price, 100.);
  EXPECT_EQ(levels.nth(2).price, 99.);
  EXPECT_FALSE(levels.empty());

  levels.erase(101.);
  EXPECT_EQ(levels.size(), 1);
  EXPECT_EQ(levels.nth(0).price, 99.);
  EXPECT_FALSE(levels.empty());

  levels.erase(99.);
  EXPECT_EQ(levels.size(), 0);
  EXPECT_TRUE(levels.empty());
}

template<class GridOrder, class MockStrategy, class Fn>
void update_order(GridOrder& order, MockStrategy& mock, Fn&&fn) {
  roq::OrderUpdate update {
      .order_id = mock.txid_.order_id,
      .routing_id = mock.txid_.routing_id()
  };
  fn(update);
  order.order_updated(update);
}

TEST(grid_order, case_insensitive) {
    MockStrategy mock;
    GridOrder<MockStrategy,  1> order {&mock};
    order.set_tick_size(1.);
    auto quotes = SingleQuote(Quote {100., 10.});
    order.modify(quotes);
    order.execute();
    EXPECT_EQ(mock.create_count, 1);
    EXPECT_EQ(mock.total_count(), 1);
    
    update_order(order, mock, [](auto& u) { u.status = OrderStatus::WORKING; });

    quotes.set_price(99.);
    order.modify(quotes);
    order.execute();
    EXPECT_EQ(mock.modify_count, 1);
    EXPECT_EQ(mock.total_count(), 2);

    update_order(order, mock, [](auto& u) { u.status = OrderStatus::WORKING; });
    
    order.execute();
}
