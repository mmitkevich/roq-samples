#pragma once

#include "grid_order.h"
#include "roq/shared/order.h"

namespace roq {
namespace shared {

template<class Self, int DIR>
template<class Quotes>
void GridOrder<Self, DIR>::modify(const Quotes& quotes) {
  for(std::size_t i=0; i<quotes.size(); i++) {
    auto quote = quotes[i];
    levels_[quote.price].desired_volume = quote.quantity;
  }
}

template<class Self, int DIR>
void GridOrder<Self, DIR>::reset() {
  for(auto& level: levels_) {
    level.desired_volume = 0;
  }
}

template<class Self, int DIR>
void GridOrder<Self, DIR>::execute() {
  price_t first_free_top = levels_.find_top([](auto& level) { return utils::compare(level.free_volume(),0.)>0; });
  price_t first_free_bottom = levels_.find_bottom([](auto& level) { return utils::compare(level.free_volume(),0.)>0; });
  
  // stupid O(2*n^2) algorithm for now
  // first pass: check excess orders
  for(auto & [txid, order]: orders_) {
    if(order.is_pending())
      continue; // don't do anything with pending orders (YET)
    // TODO: moveout tail orders first
    auto& level = levels_[order.price()];
    int cmp_volume = utils::compare(level.expected_volume(), level.desired_volume);
    if(cmp_volume>0) {
      // some extra volume - try move first
      for(auto const& dest_level: levels_) {
        if(utils::compare(dest_level.expected_volume() + order.quantity(), dest_level.desired_volume)<=0) {
          modify_order(txid, Order(Quote {
            .side = side(),
            .price = dest_level.price,
            .quantity = order.quantity()
          }));
          goto next_;
        }
      }
      // failed to move, so cancel
      cancel_order(txid);
    }
    next_:;
  }
  // second pass: add more orders if needed
  for(auto& level: levels_) {
    int cmp_volume = utils::compare(level.expected_volume(), level.desired_volume);
    if(cmp_volume<0) {
      // add order to the level
      create_order( Order(Quote {
          .side = side(),
          .price = level.price,
          .quantity = level.desired_volume - level.expected_volume()
      }));
    }
  }
}

// +PENDING_NEW
template<class Self, int DIR>
order_txid_t GridOrder<Self, DIR>::create_order(const Order& new_order) {
  auto& level = levels_[new_order.price()];
  level.pending_volume += new_order.quantity();
  auto txid = self()->next_order_txid();
  return self()->create_order(txid, orders_[txid] = Order (Quote {
    .side = side(),
    .quantity = new_order.quantity(),
    .price = new_order.price(),
  }, Order::PENDING_NEW));
}

// WORKING|PENDING_NEW|PENDING_MODIFY -> PENDING_CANCEL
template<class Self, int DIR>
order_txid_t GridOrder<Self, DIR>::cancel_order(order_txid_t txid) {
  auto& order = orders_[txid];
  auto& level = levels_[order.price()];
  level.canceling_volume += order.quantity();
  order.flags|=Order::PENDING_CANCEL;
  return self()->cancel_order(txid);
} 

// WORKING|PENDING_NEW|PENDING_MODIFY -> PENDING_CANCEL; +PENDING_MODIFY
template<class Self, int DIR>
order_txid_t GridOrder<Self, DIR>::modify_order(order_txid_t txid, const Order& new_order) {
  auto& order = orders_[txid];
  auto& level = levels_[order.price()];
  level.canceling_volume += order.quantity();
  order.flags |= Order::PENDING_CANCEL;
  auto& new_level = levels_[new_order.price()];
  new_level.pending_volume += new_order.quantity();
  auto new_txid = self()->next_order_txid(txid.order_id);
  return self()->modify_order(new_txid, orders_[new_txid] = Order ( new_order.quote, Order::PENDING_MODIFY));
}

template<class Self, int DIR>
void GridOrder<Self, DIR>::order_rejected(order_txid_t order_id, Order& order, const OrderUpdate& order_update) {
  auto& level = levels_[order.price()];
  if(order.flags&Order::PENDING_CANCEL) {
    level.canceling_volume -= order.quantity();
  }
  if(order.flags&Order::PENDING_MODIFY) {
    level.pending_volume -= order.quantity();
  }
  if(order.flags&Order::PENDING_NEW) {
    level.pending_volume -= order.quantity();
  }
  order.flags = Order::EMPTY;
}

template<class Self, int DIR>
void GridOrder<Self,DIR>::order_canceled(order_txid_t order_id, Order& order, const OrderUpdate& order_update) {
  assert(order.flags & Order::PENDING_CANCEL);
  auto& level = levels_[order.price()];
  if(order.flags&Order::PENDING_CANCEL) {
    level.canceling_volume -= order.quantity();
    level.working_volume -= order_update.remaining_quantity;
  }
  order.flags = Order::EMPTY;
}

template<class Self, int DIR>
void GridOrder<Self,DIR>::order_completed(order_txid_t order_id, Order& order, const OrderUpdate& order_update) {
  assert(order.flags & Order::WORKING);
  auto& level = levels_[order.price()];
  if(order.flags&Order::WORKING) {
    level.working_volume -= order_update.remaining_quantity;
  }
  order.flags = Order::EMPTY;
}

template<class Self, int DIR>
void GridOrder<Self,DIR>::order_working(order_txid_t order_id, Order& order, const OrderUpdate& order_update) {
  auto& level = levels_[order.price()];
  order.flags |= Order::WORKING;
}

template<class Self, int DIR>
void GridOrder<Self, DIR>::order_updated(const OrderUpdate& order_update) {
  order_txid_t id {order_update.order_id, order_update.routing_id};
  auto it = orders_.find(id);
  if(it==orders_.end())
    return;
  auto& order = it->second;
  switch(order_update.status) {
    case OrderStatus::UNDEFINED:
      assert(false);
    case OrderStatus::COMPLETED:
      order_completed(id, order, order_update);
      break;
    case OrderStatus::CANCELED:
      order_canceled(id, order, order_update);
      orders_.erase(it);
      break;
    case OrderStatus::REJECTED:
      order_rejected(id, order, order_update);
      break;
    case OrderStatus::WORKING:
      order_working(id, order, order_update);
      break;
    case OrderStatus::SENT:
    case OrderStatus::PENDING:
    case OrderStatus::ACCEPTED:
      assert(order.flags==Order::PENDING_NEW || order.flags==Order::PENDING_MODIFY);
      break;
  }
}


} // namespace shared
} // namespace roq