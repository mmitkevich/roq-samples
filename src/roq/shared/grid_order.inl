#pragma once

#include "grid_order.h"
#include "roq/shared/order.h"
#include "roq/logging.h"

namespace roq {
namespace shared {

using namespace roq::literals;

template<class Self, int DIR>
template<class Quotes>
void GridOrder<Self, DIR>::modify(const Quotes& quotes) {
  log::trace_1("GridOrder<{}>::modify quotes {}"_fmt, quotes);
  for(auto& level: levels_) {
    level.desired_volume = 0.;
  }
  std::size_t sze = quotes.size();
  for(std::size_t i=0; i<sze; i++) {
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
      for(auto & dest_level: levels_) {
        if(utils::compare(dest_level.expected_volume() + order.quantity(), dest_level.desired_volume)<=0) {
          dest_level.pending_volume += order.quantity();
          level.canceling_volume += order.quantity();
          pending_orders_.emplace_back(txid, LimitOrder(Quote {
            .side = side(),
            .price = dest_level.price,
            .quantity = order.quantity()
          }, LimitOrder::PENDING_MODIFY));
          goto next_;
        }
      }
      // failed to move, so cancel
      level.canceling_volume += order.quantity();
      cancel_order(txid);
    }
    next_:;
  }
  // second pass: add more orders if needed
  for(auto it = std::begin(levels_); it!= std::end(levels_); it++) {
    auto& level = *it;
    int cmp_volume = utils::compare(level.expected_volume(), level.desired_volume);
    if(cmp_volume<0) {
      auto qty = level.desired_volume - level.expected_volume();
      level.pending_volume += qty;
      // add order to the level
      pending_orders_.emplace_back(self()->next_order_txid(), LimitOrder(Quote {
          .side = side(),
          .price = level.price,
          .quantity = qty
      }, LimitOrder::PENDING_NEW));
    }
  }
  // flush the queue
  while(!pending_orders_.empty()) {
    auto& [txid, order] = pending_orders_.front();
    if(order.flags.test(LimitOrder::PENDING_MODIFY)) {
      modify_order(txid, order);
    } else {
      create_order(txid, order);
    }
    pending_orders_.pop_front();
  }
}

// +PENDING_NEW
template<class Self, int DIR>
order_txid_t GridOrder<Self, DIR>::create_order(order_txid_t id, const LimitOrder& new_order) {
  //auto& level = levels_[new_order.price()];
  //level.pending_volume += new_order.quantity();
  auto& order = orders_[id] = LimitOrder (Quote {
    .side = side(),
    .price = new_order.price(),    
    .quantity = new_order.quantity()
  }, LimitOrder::PENDING_NEW);
  log::trace_1("GridOrder<{}>::create_order: order_id:{}, routing_id:{}, order: {{{}}}"_fmt,
   side(), id.order_id, id.routing_id_, order, *this);
  log::trace_2("GridOrder<{}>: {}"_fmt, side(), *this);
  auto ret = self()->create_order(id, order);
  return ret;
}

// WORKING|PENDING_NEW|PENDING_MODIFY -> PENDING_CANCEL
template<class Self, int DIR>
order_txid_t GridOrder<Self, DIR>::cancel_order(order_txid_t id) {
  auto& order = orders_[id];
  //auto& level = levels_[order.price()];
  //level.canceling_volume += order.quantity();
  order.flags_set(LimitOrder::PENDING_CANCEL);
  log::trace_1("GridOrder<{}>::cancel_order: order_id:{}, routing_id:{}, order:{{{}}}"_fmt, side(),id.order_id, id.routing_id_, order);
  log::trace_2("GridOrder<{}>:{}"_fmt, side(), *this);  
  auto ret = self()->cancel_order(id, order);
  return ret;
} 

// WORKING|PENDING_NEW|PENDING_MODIFY -> PENDING_CANCEL; +PENDING_MODIFY
template<class Self, int DIR>
order_txid_t GridOrder<Self, DIR>::modify_order(order_txid_t id, const  LimitOrder& new_order) {
  auto& order = orders_[id];
  //auto& level = levels_[order.price()];
  //level.canceling_volume += order.quantity();
  order.flags.set(LimitOrder::PENDING_CANCEL);
  auto& new_level = levels_[new_order.price()];
  //new_level.pending_volume += new_order.quantity();
  auto new_id = self()->next_order_txid(id.order_id);
  auto& modified_order = orders_[new_id] = LimitOrder ( new_order.quote, LimitOrder::PENDING_MODIFY);
  modified_order.prev_routing_id = id.routing_id_;
  log::trace_1("GridOrder<{}>::modify_order: order_id:{}, routing_id:{}, prev_routing_id:{}, new_order:{{{}}}, prev_order:{{{}}}"_fmt,
    side(), new_id.order_id, new_id.routing_id_,  id.routing_id_,  modified_order, order);
  log::trace_2("GridOrder<{}>:{}"_fmt, side(), *this);
  auto ret = self()->modify_order(new_id, modified_order);
  return ret;
}

template<class Self, int DIR>
void GridOrder<Self, DIR>::order_rejected(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
  auto& level = levels_[order.price()];
  if(order.flags&LimitOrder::PENDING_CANCEL) {
    level.canceling_volume -= order.quantity();
  }
  if(order.flags&LimitOrder::PENDING_MODIFY) {
    level.pending_volume -= order.quantity();
  }
  if(order.flags&LimitOrder::PENDING_NEW) {
    level.pending_volume -= order.quantity();
  }
  order.flags = LimitOrder::EMPTY;
  log::trace_1("GridOrder<{}>::order_rejected: {}"_fmt, side(), *this);
}

template<class Self, int DIR>
void GridOrder<Self,DIR>::order_canceled(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
  assert(order.flags & LimitOrder::PENDING_CANCEL);
  auto& level = levels_[order.price()];
  if(order.flags&LimitOrder::PENDING_CANCEL) {
    level.canceling_volume -= order.quantity();
    level.working_volume -= order_update.remaining_quantity;
  } else {
    assert(false);
  }
  order.flags = LimitOrder::EMPTY;

  log::trace_1("GridOrder<{}>::order_canceled: order_id:{}, routing_id:{}, order:{}"_fmt, side(), id.order_id, id.routing_id_, order);
  log::trace_2("GridOrder<{}>:{}"_fmt, side(), *this);
}

template<class Self, int DIR>
void GridOrder<Self,DIR>::order_completed(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
  auto& level = levels_[order.price()];
  assert(order.flags.test(LimitOrder::WORKING));  
  if(order.flags.test(LimitOrder::WORKING)) {
    order.flags.set(LimitOrder::WORKING);
    level.working_volume -= order_update.remaining_quantity;
  }
  order.flags = LimitOrder::EMPTY;
  log::trace_1("GridOrder<{}>::order_completed: {}"_fmt, side(), order);
  log::trace_2("GridOrder<{}>:{}"_fmt,side(), *this);
}

template<class Self, int DIR>
void GridOrder<Self,DIR>::order_working(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
  auto& level = levels_[order.price()];
  assert(order.flags.test(LimitOrder::PENDING_NEW|LimitOrder::PENDING_MODIFY));  
  assert(!order.flags.test(LimitOrder::WORKING));  
  if(order.flags.test(LimitOrder::PENDING_NEW)) {
    order.flags.reset(LimitOrder::PENDING_NEW);
    level.pending_volume -= order_update.remaining_quantity;
    order.flags.set(LimitOrder::WORKING);  
    level.working_volume += order_update.remaining_quantity;
  }
  if(order.flags.test(LimitOrder::PENDING_MODIFY)) {
    order.flags.reset(LimitOrder::PENDING_MODIFY);
    level.pending_volume -= order_update.remaining_quantity;
    order.flags.set(LimitOrder::WORKING);  
    level.working_volume += order_update.remaining_quantity;

    assert(order.prev_routing_id!=undefined_order_id);
    auto prev_id = order_txid_t(id.order_id, order.prev_routing_id);
    assert(orders_.contains(prev_id));
    auto& prev_order = orders_[prev_id];
    auto& prev_level = levels_[prev_order.price()];
    assert(prev_order.flags.test(LimitOrder::PENDING_CANCEL) && prev_order.flags.test(LimitOrder::WORKING));
    prev_order.flags.reset(LimitOrder::PENDING_CANCEL|LimitOrder::WORKING);
    prev_level.canceling_volume -= prev_order.quantity();
    prev_level.working_volume -= prev_order.quantity();
    assert(prev_order.flags == LimitOrder::EMPTY);
    log::trace_1("GridOrder<{}>::erase_order order_id:{}, routing_id:{}"_fmt, 
      side(), prev_id.order_id, prev_id.routing_id_);
    orders_.erase(prev_id);
    levels_.shrink();    
  }
  log::trace_1("GridOrder<{}>::order_working: {}"_fmt, side(), order);
  log::trace_2("GridOrder<{}>:{}"_fmt, side(), *this);
}

template<class Self, int DIR>
void GridOrder<Self, DIR>::order_updated(const OrderUpdate& order_update) {
  order_txid_t id {order_update.order_id, order_update.routing_id};
  log::trace_1("GridOrder<{}>::order_updated: order_id: {}, routing_id: {}, order_update: {}"_fmt, side(), id.order_id, id.routing_id_, order_update);
  log::trace_2("GridOrder<{}>:{}"_fmt, side(), *this);      
  auto it = orders_.find(id);
  if(it==orders_.end()) {
    log::warn("GridOrder<{}>::order_updated: order_id: {}, routing_id:{}  not found"_fmt, side(), id.order_id, id.routing_id_);
    return;
  }
  
  auto& order = it->second;  

  switch(order_update.status) {
    case OrderStatus::UNDEFINED:
      assert(false);
    case OrderStatus::COMPLETED:
      order_completed(id, order, order_update);
      break;
    case OrderStatus::CANCELED:
      order_canceled(id, order, order_update);
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
      assert(order.flags==LimitOrder::PENDING_NEW || order.flags==LimitOrder::PENDING_MODIFY);
      break;
  }
}


} // namespace shared
} // namespace roq