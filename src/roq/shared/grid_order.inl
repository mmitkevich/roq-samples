#pragma once

#include "grid_order.h"
#include "roq/shared/order.h"
#include "roq/logging.h"

#include "roq/shared/price.h"
#include "roq/shared/strategy.inl"
#include "roq/shared/order_map.inl"
namespace roq {
inline namespace shared {

using namespace roq::literals;

template<int DIR>
template<class Quotes>
void GridOrder<DIR>::modify(const Quotes& quotes) {
  log::debug("GridOrder<{}>::modify quotes {}"_sv, quotes);
  for(auto& level: levels_) {
    level.desired_volume = 0.;
  }
  std::size_t sze = quotes.size();
  for(std::size_t i=0; i<sze; i++) {
    auto quote = quotes[i];
    if(!is_undefined_price(quote.price()))
      levels_[quote.price()].desired_volume = quote.quantity();
  }
}

template<int DIR>
void GridOrder<DIR>::reset() {
  for(auto& level: levels_) {
    level.desired_volume = 0;
  }
}


template<int DIR>
template<class Context>
void GridOrder<DIR>::execute(Context& context) {
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
          orders_.modify_order(txid, LimitOrder(Quote {
            .side_ = side(),
            .price_ = dest_level.price,
            .quantity_ = order.quantity()
          }), context);
          goto next_;
        }
      }
      // failed to move, so cancel
      level.canceling_volume += order.quantity();
      orders_.cancel_order(txid, context);
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
      orders_.create_order(context.next_order_txid(), LimitOrder(Quote {
          .side_ = side(),
          .price_ = level.price,
          .quantity_ = qty
      }), context);
    }
  }
  orders_.flush_orders(context);
}


template<int DIR>
void GridOrder<DIR>::order_rejected(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
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
  log::debug("GridOrder<{}>::order_rejected: {}"_sv, side(), *this);
}

template<int DIR>
void GridOrder<DIR>::order_canceled(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
  assert(order.flags & LimitOrder::PENDING_CANCEL);
  auto& level = levels_[order.price()];
  if(order.flags&LimitOrder::PENDING_CANCEL) {
    level.canceling_volume -= order.quantity();
    level.working_volume -= order_update.remaining_quantity;
  } else {
    assert(false);
  }
  order.flags = LimitOrder::EMPTY;

  log::debug("GridOrder<{}>::order_canceled: order_id:{}, routing_id:{}, order:{}"_sv, side(), id.order_id, id.routing_id_, order);
  log::debug("GridOrder<{}>: {}"_sv, side(), *this);
}

template<int DIR>
void GridOrder<DIR>::order_completed(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
  auto& level = levels_[order.price()];
  assert(order.flags.test(LimitOrder::WORKING));  
  if(order.flags.test(LimitOrder::WORKING)) {
    order.flags.set(LimitOrder::WORKING);
    level.working_volume -= order_update.remaining_quantity;
  }
  order.flags = LimitOrder::EMPTY;
  log::debug("GridOrder<{}>::order_completed: {}"_sv, side(), order);
  log::debug("GridOrder<{}>:{}"_sv,side(), *this);
}

template<int DIR>
void GridOrder<DIR>::order_working(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update) {
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
    log::debug("GridOrder<{}>::erase_order order_id:{}, routing_id:{}"_sv, 
      side(), prev_id.order_id, prev_id.routing_id_);
    orders_.erase(prev_id);
    levels_.shrink();    
  }
  log::debug("GridOrder<{}>::order_working: {}"_sv, side(), order);
  log::debug("GridOrder<{}>:{}"_sv, side(), *this);
}

template<int DIR>
void GridOrder<DIR>::order_updated(const OrderUpdate& order_update) {
  order_txid_t id {order_update.order_id, order_update.routing_id};
  log::debug("GridOrder<{}>::order_updated: order_id: {}, routing_id: {}, order_update: {}"_sv, side(), id.order_id, id.routing_id_, order_update);
  log::debug("GridOrder<{}>:{}"_sv, side(), *this);      
  auto it = orders_.find(id);
  if(it==orders_.end()) {
    log::warn("GridOrder<{}>::order_updated: order_id: {}, routing_id:{}  not found"_sv, side(), id.order_id, id.routing_id_);
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
    case OrderStatus::ACCEPTED:
      assert(order.flags==LimitOrder::PENDING_NEW || order.flags==LimitOrder::PENDING_MODIFY);
      break;
  }
}


} // namespace shared
} // namespace roq