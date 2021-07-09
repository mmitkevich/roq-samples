
#include <gtest/gtest.h>
#include <roq/order_status.h>
#include <roq/order_update.h>
#include <iostream>
#include "roq/shared/grid_order.h"
#include "roq/format.h"
#include "roq/shared/quote.h"
#include "roq/utils/compare.h"
#include "roq/shared/grid_order.inl"
#include "absl/container/flat_hash_map.h"
#include "absl/container/btree_map.h"
#include "roq/shared/order_map.h"

using namespace roq;
using namespace roq::utils;

struct MockStrategy {
  using Self = MockStrategy;

  template<class... ArgsT>
  void update_order(Side side, roq::OrderUpdate&& update) {
    if(side==Side::BUY)
      bid.order_updated(std::move(update));
    else if(side==Side::SELL)
      ask.order_updated(std::move(update));
    else assert(false);
  }

  template<int dir>
  auto& get_order();
    
  shared::order_txid_t create_order(shared::order_txid_t id, const shared::LimitOrder& order) {
    create_count++;
    log::info("MockStrategy::create_order ( order_id: {}, routing_id: {}, side: {}, price: {}, qty: {}, flags: {} )"_sv
      , id.order_id, id.routing_id_, order.side(), order.price(), order.quantity(), order.flags);
    update_order(order.side(), roq::OrderUpdate {
      .order_id = id.order_id,
      .status = OrderStatus::WORKING,  
      .remaining_quantity = order.quantity(),
      .routing_id = id.routing_id(),      
    });        
    return id;
  }

  shared::order_txid_t cancel_order(shared::order_txid_t id, const shared::LimitOrder& order) {
    cancel_count++;
    log::info("MockStrategy::cancel_order ( order_id: {}, routing_id: {}, side: {}, price: {}, qty: {}, flags: {}  )\n"_sv
      , id.order_id, id.routing_id_, order.side(), order.price(), order.quantity(), order.flags);
    update_order(order.side(), roq::OrderUpdate {
      .order_id = id.order_id,
      .status = OrderStatus::CANCELED,  
      .remaining_quantity = order.quantity(),
      .routing_id = id.routing_id()      
    });         
    return id;
  }

  //! returns new client order id
  shared::order_txid_t modify_order(shared::order_txid_t id, const shared::LimitOrder& order) {
    modify_count++;
    log::info("MockStrategy::modify_order ( order_id: {}, routing_id: {}, side: {}, price: {}, qty: {}, prev_routing_id:{}, flags: {} )\n"_sv
      , id.order_id, id.routing_id_, order.side(), order.price(), order.quantity(), order.prev_routing_id, order.flags);      
    update_order(order.side(), roq::OrderUpdate {
      .order_id = id.order_id,
      .status = OrderStatus::WORKING,  
      .remaining_quantity = order.quantity(),
      .routing_id = id.routing_id()
    });      
    return id;
  }

  // new order, new tx
  shared::order_txid_t next_order_txid() { 
    ++next_txid_.order_id;
    ++next_txid_.routing_id_;
    return next_txid_; 
  }
  
  // same order, new tx
  shared::order_txid_t next_order_txid(order_id_t id) { 
    order_txid_t txid {id, ++next_txid_.routing_id_};
    return txid; 
  }

  shared::order_txid_t next_txid_;
  
  constexpr static shared::price_t s_tick_size = 1.0;

  GridOrder<1> bid{orders, s_tick_size};
  GridOrder<-1> ask{orders, s_tick_size};
  LimitOrdersMap orders;
  int create_count = 0;
  int modify_count = 0;
  int cancel_count = 0;
  int total_count() { return create_count+modify_count+cancel_count; }
};

template<> 
auto& MockStrategy::get_order<1>() { return bid;}
template<> 
auto& MockStrategy::get_order<-1>() { return ask;}

TEST(grid_order, levels_container) {
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
  EXPECT_EQ(i,3);

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

template<class GridOrder>
bool grid_equals(GridOrder& grid, std::map<price_t, volume_t>&& expected) {
  bool ret = true;
  for(auto& [id, order]: grid.orders()) {
    assert(!order.is_pending());
    assert(!order.is_pending_cancel());
    assert(!order.empty());
    expected[order.price()] -= order.quantity();
  }
  for(auto it =  expected.begin(); it!=expected.end();) {
    auto [price, qty] = *it;
    EXPECT_EQ(qty, 0.);
    ret = ret && utils::compare(qty,0)==0;
    if(utils::compare(qty,0)==0) {
      it = expected.erase(it);
    } else {
      it++;
    }
  }
  EXPECT_TRUE(ret = ret && expected.empty());
  for(auto& e: expected) {
    log::info("unexpected price:{}, qty:{}"_sv, e.first, e.second);
  }
  return ret;
}

TEST(grid_order, one_bid_move_down) {
    MockStrategy s;
    auto quotes = make_quotes<1>({Quote {Side::BUY, 100., 10.}});
    s.bid.modify(quotes);
    s.bid.execute(s);
    EXPECT_EQ(s.create_count, 1);
    EXPECT_EQ(s.total_count(), 1);
    EXPECT_TRUE(grid_equals(s.bid, {{100.,10.}}));
    quotes.set_price(99.);
    s.bid.modify(quotes);
    s.bid.execute(s);
    EXPECT_EQ(s.modify_count, 1);
    EXPECT_EQ(s.total_count(), 2);
    EXPECT_TRUE(grid_equals(s.bid, {{99.,10.}}));
}

TEST(grid_order, one_bid_move_up) {
    MockStrategy s;
    auto quotes = make_quotes<1>({Quote {Side::BUY, 100., 10.}});
    s.bid.modify(quotes);
    s.bid.execute(s);
    EXPECT_EQ(s.create_count, 1);
    EXPECT_EQ(s.total_count(), 1);
    EXPECT_TRUE(grid_equals(s.bid, {{100.,10.}}));

    quotes.set_price(101.);
    s.bid.modify(quotes);
    s.bid.execute(s);
    EXPECT_EQ(s.modify_count, 1);
    EXPECT_EQ(s.total_count(), 2);
    EXPECT_TRUE(grid_equals(s.bid, {{101.,10.}}));
}

TEST(grid_order, one_ask_move_up) {
    MockStrategy s;
    auto quotes = make_quotes<1>({Quote {Side::SELL, 100., 10.}});
    s.ask.modify(quotes);
    s.ask.execute(s);
    EXPECT_EQ(s.create_count, 1);
    EXPECT_EQ(s.total_count(), 1);
    EXPECT_TRUE(grid_equals(s.ask, {{100.,10.}}));
    quotes.set_price(101.);
    s.ask.modify(quotes);
    s.ask.execute(s);
    EXPECT_EQ(s.modify_count, 1);
    EXPECT_EQ(s.total_count(), 2);
    EXPECT_TRUE(grid_equals(s.ask, {{101.,10.}}));
}

TEST(grid_order, one_ask_move_down) {
    MockStrategy s;
    auto quotes = make_quotes<1>({Quote {Side::SELL, 100., 10.}});
    s.ask.modify(quotes);
    s.ask.execute(s);
    EXPECT_EQ(s.create_count, 1);
    EXPECT_EQ(s.total_count(), 1);
    EXPECT_TRUE(grid_equals(s.ask, {{100.,10.}}));
    quotes.set_price(99.);
    s.ask.modify(quotes);
    s.ask.execute(s);
    EXPECT_EQ(s.modify_count, 1);
    EXPECT_EQ(s.total_count(), 2);
    EXPECT_TRUE(grid_equals(s.ask, {{99.,10.}}));
}

TEST(grid_order, three_bids_move_down) {
    MockStrategy s;
    
    auto quotes = GridQuote(Side::BUY, 100., 9).set_tick(1., 3.);
    s.bid.modify(quotes);
    s.bid.execute(s);
    log::info("bids: {}"_sv, s.bid);
    EXPECT_EQ(s.create_count, 3);
    EXPECT_EQ(s.total_count(), 3);
    EXPECT_TRUE(grid_equals(s.bid, {{100.,3.},{99.,3.},{98.,3.}}));
    quotes.set_price(99.);
    s.bid.modify(quotes);
    s.bid.execute(s);
    log::info("bids: {}"_sv, s.bid);
    EXPECT_EQ(s.modify_count, 1);
    EXPECT_EQ(s.total_count(), 4);
    EXPECT_TRUE(grid_equals(s.bid, {{99.,3.},{98.,3.},{97.,3.}}));
}

TEST(grid_order, three_asks_move_up) {
    MockStrategy s;
    
    auto quotes = GridQuote(Side::SELL, 100., 9).set_tick(1.,3.);
    s.ask.modify(quotes);
    s.ask.execute(s);
    log::info("asks: {}"_sv, s.ask);
    EXPECT_EQ(s.create_count, 3);
    EXPECT_EQ(s.total_count(), 3);
    EXPECT_TRUE(grid_equals(s.ask, {{100.,3.},{101.,3.},{102.,3.}}));
    quotes.set_price(150.);
    s.ask.modify(quotes);
    s.ask.execute(s);
    log::info("asks: {}"_sv, s.ask);
    EXPECT_EQ(s.modify_count, 3);
    EXPECT_EQ(s.total_count(), 6);
    EXPECT_TRUE(grid_equals(s.ask, {{150.,3.},{151.,3.},{152.,3.}}));
}