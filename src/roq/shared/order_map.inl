#pragma once
#include "order_map.h"
#include "roq/logging.h"

namespace roq {
inline namespace shared {


template<class Context>
order_txid_t LimitOrdersMap::create_order(order_txid_t id, const LimitOrder& new_order, Context& context) {
  auto& pending_order = pending_orders_.emplace_back(id, new_order);
  pending_order.second.flags.set(LimitOrder::PENDING_NEW);
  return id;
}

template<class Context>
order_txid_t LimitOrdersMap::do_create_order(order_txid_t id, const LimitOrder& new_order, Context& context) {
  auto& order = operator[](id) = new_order;
  order.flags = LimitOrder::PENDING_NEW;
  log::trace_1("OrderMap::create_order: order_id:{}, routing_id:{}, order: {{{}}}"_fmt,
   id.order_id, id.routing_id_, order, *this);
  log::trace_2("OrderMap: {}"_fmt, *this);
  auto ret = context.create_order(id, order);
  return ret;
}

// WORKING|PENDING_NEW|PENDING_MODIFY -> PENDING_CANCEL
template<class Context>
order_txid_t LimitOrdersMap::cancel_order(order_txid_t id, Context& context) {
  auto& order = operator[](id);
  order.flags.set(LimitOrder::PENDING_CANCEL);
  log::trace_1("OrderMap::cancel_order: order_id:{}, routing_id:{}, order:{{{}}}"_fmt,
   id.order_id, id.routing_id_, order);
  log::trace_2("OrderMap:{}"_fmt, *this);  
  auto ret = context.cancel_order(id, order);
  return ret;
} 

template<class Context>
order_txid_t LimitOrdersMap::modify_order(order_txid_t id, const  LimitOrder& new_order, Context& context) {
  auto& pending_order = pending_orders_.emplace_back(id, new_order);
  pending_order.second.flags.set(LimitOrder::PENDING_MODIFY);
  return id;
}

template<class Context>
order_txid_t LimitOrdersMap::do_modify_order(order_txid_t id, const  LimitOrder& new_order, Context& context) {
  auto& order = operator[](id);
  order.flags.set(LimitOrder::PENDING_CANCEL);
  auto new_id = context.next_order_txid(id.order_id);
  auto& modified_order = operator[](new_id) = LimitOrder ( new_order, LimitOrder::PENDING_MODIFY);
  modified_order.prev_routing_id = id.routing_id_;
  log::trace_1("OrderMap::modify_order: order_id:{}, routing_id:{}, prev_routing_id:{}, new_order:{{{}}}, prev_order:{{{}}}"_fmt,
    new_id.order_id, new_id.routing_id_,  id.routing_id_,  modified_order, order);
  log::trace_2("OrderMap:{}"_fmt, *this);
  auto ret = context.modify_order(new_id, modified_order);
  return ret;
}
template<class Context>
void LimitOrdersMap::flush_orders(Context& context) {
  // flush the queue
  while(!pending_orders_.empty()) {
    auto& [txid, order] = pending_orders_.front();
    if(order.flags.test(LimitOrder::PENDING_MODIFY)) {
      do_modify_order(txid, order, context);
    } else if(order.flags.test(LimitOrder::PENDING_NEW)) {
      do_create_order(txid, order, context);
    } else {
      assert(false);
    }
    pending_orders_.pop_front();
  }
}

} // namespace shared
} // namespace roq