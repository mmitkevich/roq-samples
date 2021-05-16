#pragma once

#include <limits>

#include "roq/shared/order.h"
#include "roq/shared/quote.h"
#include "roq/utils/common.h"
#include "roq/utils/update.h"
#include "roq/logging.h"
#include "roq/shared/grid_order.inl"
#include "roq/shared/strategy.h"


namespace roq {
namespace shared {


using namespace roq::literals;

//Strategy::Strategy(client::Dispatcher &dispatcher)
//  : dispatcher_(dispatcher)
//{
  //instruments_.emplace_back(*this, Flags::exchange(), Flags::symbol(), Flags::account());
//}

namespace {
  ROQ_DECLARE_HAS_MEMBER(symbol);
  ROQ_DECLARE_HAS_MEMBER(exchange);
}
template<class Self>
template<typename T>
void Strategy<Self>::dispatch(const T& event) {
  if constexpr(ROQ_HAS_MEMBER(symbol,T) && ROQ_HAS_MEMBER(exchange, T)) {
    auto id = instrument_id(event);
    if(id!=undefined_instrument_id) {
      assert(event.message_info.source == 0u);
      assert(id>=0 && id<instruments_.size());
      instruments_[id](event.value);
    }
  } else {
    for(auto &ins: instruments_) {
      ins(event.value);
    }
  }
}

template<class Self>
void Strategy<Self>::operator()(const Event<Timer> &event) {
  // note! using system clock (*not* the wall clock)
  if (event.value.now < next_sample_)
    return;
  if (next_sample_ != next_sample_.zero())  // initialized?
    self()->model().time_updated(*this);
  auto now = std::chrono::duration_cast<std::chrono::seconds>(event.value.now);
  next_sample_ = now + self()->sample_freq();
  // possible extension: reset request timeout
}

template<class Self>
void Strategy<Self>::operator()(const Event<Connected> &event) {
  self()->dispatch(event);
}

template<class Self>
void Strategy<Self>::operator()(const Event<Disconnected> &event) {
  self()->dispatch(event);
}

template<class Self>
void Strategy<Self>::operator()(const Event<DownloadBegin> &event) {
  self()->dispatch(event);
}

template<class Self>
void Strategy<Self>::operator()(const Event<DownloadEnd> &event) {
  self()->dispatch(event);
  if (utils::update(txid_.order_id, event.value.max_order_id)) {
    log::info("max_order_id={}"_fmt, txid_.order_id);
  }
}

template<class Self>
void Strategy<Self>::operator()(const Event<GatewayStatus> &event) {
  self()->dispatch(event);
}

template<class Self>
void Strategy<Self>::operator()(const Event<ReferenceData> &event) {
  self()->dispatch(event);
}

template<class Self>
void Strategy<Self>::operator()(const Event<MarketStatus> &event) {
  self()->dispatch(event);
}

template<class Self>
void Strategy<Self>::operator()(const Event<MarketByPriceUpdate> &event) {
  self()->dispatch(event);
  auto iid = to_instrument_id(event.value);
  self()->model().quotes_updated(*this, iid);
}

template<class Self>
void Strategy<Self>::operator()(const Event<OrderAck> &event) {
  log::info("OrderAck={}"_fmt, event.value);
  auto &order_ack = event.value;
  if (utils::is_request_complete(order_ack.status)) {
    // possible extension: reset request timeout
  }
}

template<class Self>
void Strategy<Self>::Instrument::operator()(const OrderUpdate &order_update) {
  switch(order_update.side) {
    case Side::BUY:  buy_order_.order_updated(order_update); break;
    case Side::SELL: sell_order_.order_updated(order_update); break;
    default: assert(false);
  }
}

template<class Self>
void Strategy<Self>::operator()(const Event<OrderUpdate> &event) {
  log::info("OrderUpdate={}"_fmt, event.value);
  self()->dispatch(event);  // updates position & quotes
  instrument_id_t iid = to_instrument_id(event.value);
  assert(iid!=undefined_instrument_id);
  self()->model().position_updated(iid, *this);
}

template<class Self>
void Strategy<Self>::operator()(const Event<TradeUpdate> &event) {
  log::info("TradeUpdate={}"_fmt, event.value);
}

template<class Self>
void Strategy<Self>::operator()(const Event<PositionUpdate> &event) {
  self()->dispatch(event);
  instrument_id_t iid = to_instrument_id(event.value);
  assert(iid!=undefined_instrument_id);
  self()->model().position_updated(iid, *this);  
}

template<class Self>
void Strategy<Self>::operator()(const Event<FundsUpdate> &event) {
  log::info("FundsUpdate={}"_fmt, event.value);
}

template<class Self>
order_txid_t Strategy<Self>::create_order(order_txid_t id, const LimitOrder& order) {
  if (!self()->enable_trading()) {
    log::warn("Trading *NOT* enabled"_sv);
    return {};
  }
  dispatcher_.send(
    roq::CreateOrder {
          .account = self()->account(),
          .order_id = id.order_id,          
          .exchange = self()->exchange(),
          .symbol = self()->symbol(),
          .side = order.side(),
          .quantity = order.quantity(),
          .order_type = OrderType::LIMIT,
          .price = order.price(),
          .time_in_force = TimeInForce::GTC,
          .position_effect = {},
          .execution_instruction = {},
          .stop_price = NaN,
          .max_show_quantity = NaN,
          .order_template = {},
          .routing_id = id.routing_id(),
    },
    0u);
  return id;
}

template<class Self>
order_txid_t Strategy<Self>::cancel_order(order_txid_t id, const LimitOrder& order) {
  if (!self()->enable_trading()) {
    log::warn("Trading *NOT* enabled"_sv);
    return {};
  }  
  dispatcher_.send(
    roq::CancelOrder{
        .account = self()->account(),
        .order_id = id.order_id,
        //.routing_id = existing_order_id.routing_id()
    },
    0u);
  return id;
}

template<class Self>
order_txid_t Strategy<Self>::modify_order(order_txid_t id, const LimitOrder& order) {
  if (!self()->enable_trading()) {
    log::warn("Trading *NOT* enabled"_sv);
    return {};
  }  
  dispatcher_.send(
    roq::ModifyOrder{
        .account = self()->account(),
        .order_id = id.order_id,
        .quantity = order.quantity(),
        .price = order.price(),
        //.routing_id = id.routing_id()
    },
    0u);
  return id;
}

}  // namespace shared
}  // namespace roq
