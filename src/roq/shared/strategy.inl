#pragma once

#include <limits>

#include "roq/shared/instrument.h"
#include "roq/shared/order.h"
#include "roq/shared/quote.h"
#include "roq/utils/common.h"
#include "roq/utils/update.h"
#include "roq/logging.h"
#include "roq/shared/grid_order.inl"
#include "roq/shared/strategy.h"


namespace roq {
inline namespace shared {

template<class Instrument>
template<typename T>
int Instruments<Instrument>::dispatch(const T& event) {
  auto sym = SymbolView(event.symbol, event.exchange);
  assert(event.message_info.source == 0u);
  auto id = find_id(sym);
  if(id!=undefined_order_id) {
    instruments_[id](event);
    return 1;
  }
}
template<class Instrument>
template<typename T>
int Instruments<Instrument>::broadcast(const T& event) {
  for(auto& ins: instruments_) {
    ins(event);
  }
  return instruments_.size();
}

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

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<Timer> &event) {
  // note! using system clock (*not* the wall clock)
  if (event.value.now < next_sample_)
    return;
  if (next_sample_ != next_sample_.zero())  // initialized?
    self()->model().time_updated(*this);
  auto now = std::chrono::duration_cast<std::chrono::seconds>(event.value.now);
  next_sample_ = now + self()->sample_freq();
  // possible extension: reset request timeout
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<Connected> &event) {
  int n = self()->dispatch(event);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<Disconnected> &event) {
  int n =self()->dispatch(event);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<DownloadBegin> &event) {
  self()->dispatch(event);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<DownloadEnd> &event) {
  self()->dispatch(event);
  if (utils::update(txid_.order_id, event.value.max_order_id)) {
    log::info("max_order_id={}"_sv, txid_.order_id);
  }
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<GatewayStatus> &event) {
  self()->dispatch(event);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<ReferenceData> &event) {
  self()->dispatch(event);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<MarketStatus> &event) {
  self()->dispatch(event);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<MarketByPriceUpdate> &event) {
  self()->dispatch(event);
  auto iid = instruments_.find_id(SymbolView::from(event));
  if(iid != undefined_instrument_id) {
    self()->model().quotes_updated(*this, iid);
  }
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<OrderAck> &event) {
  log::info("OrderAck={}"_sv, event.value);
  auto &order_ack = event.value;
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<OrderUpdate> &event) {
  log::info("OrderUpdate={}"_sv, event.value);
  self()->dispatch(event);  // updates position & quotes
  instrument_id_t iid = instruments_.find_id(SymbolView::from(event));
  assert(iid!=undefined_instrument_id);
  self()->model().position_updated(iid, *this);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<TradeUpdate> &event) {
  log::info("TradeUpdate={}"_sv, event.value);
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<PositionUpdate> &event) {
  self()->dispatch(event);
  instrument_id_t iid = instruments_.find_id(SymbolView::from(event));
  assert(iid!=undefined_instrument_id);
  self()->model().position_updated(iid, *this);  
}

template<class Self, class Instrument>
void Strategy<Self, Instrument>::operator()(const Event<FundsUpdate> &event) {
  log::info("FundsUpdate={}"_sv, event.value);
}

template<class Self, class Instrument>
order_txid_t Strategy<Self, Instrument>::create_order(order_txid_t id, const LimitOrder& order) {
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

template<class Self, class Instrument>
order_txid_t Strategy<Self, Instrument>::cancel_order(order_txid_t id, const LimitOrder& order) {
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

template<class Self, class Instrument>
order_txid_t Strategy<Self, Instrument>::modify_order(order_txid_t id, const LimitOrder& order) {
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
