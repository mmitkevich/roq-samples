/* Copyright (c) 2017-2021, Hans Erik Thrane */
#include <limits>

#include "roq/utils/common.h"
#include "roq/utils/update.h"

#include "roq/logging.h"

#include "roq/shared/grid_order.inl"

#include "strategy.h"
#include "flags.h"

using namespace roq::literals;

namespace roq {
namespace mmaker {


Strategy::Strategy(client::Dispatcher &dispatcher)
  : dispatcher_(dispatcher)
  , instrument_(Flags::exchange(), Flags::symbol(), Flags::account()) {
}

void Strategy::operator()(const Event<Timer> &event) {
  // note! using system clock (*not* the wall clock)
  if (event.value.now < next_sample_)
    return;
  if (next_sample_ != next_sample_.zero())  // initialized?
    update();
  auto now = std::chrono::duration_cast<std::chrono::seconds>(event.value.now);
  next_sample_ = now + std::chrono::seconds{Flags::sample_freq_secs()};
  // possible extension: reset request timeout
}

void Strategy::operator()(const Event<Connected> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<Disconnected> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<DownloadBegin> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<DownloadEnd> &event) {
  dispatch(event);
  if (utils::update(max_order_id_, event.value.max_order_id)) {
    log::info("max_order_id={}"_fmt, max_order_id_);
  }
}

void Strategy::operator()(const Event<GatewayStatus> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<ReferenceData> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<MarketStatus> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<MarketByPriceUpdate> &event) {
  dispatch(event);
  update();
}

void Strategy::operator()(const Event<OrderAck> &event) {
  log::info("OrderAck={}"_fmt, event.value);
  auto &order_ack = event.value;
  if (utils::is_request_complete(order_ack.status)) {
    // possible extension: reset request timeout
  }
}

void Strategy::operator()(const Event<OrderUpdate> &event) {
  log::info("OrderUpdate={}"_fmt, event.value);
  dispatch(event);  // update position
  auto &order_update = event.value;
  if (utils::is_order_complete(order_update.status)) {
    switch(order_update.side) {
      case Side::BUY:  bid_.order_updated(order_update); break;
      case Side::SELL: ask_.order_updated(order_update); break;
      default: assert(false);
    }
  }
}

void Strategy::operator()(const Event<TradeUpdate> &event) {
  log::info("TradeUpdate={}"_fmt, event.value);
}

void Strategy::operator()(const Event<PositionUpdate> &event) {
  dispatch(event);
}

void Strategy::operator()(const Event<FundsUpdate> &event) {
  log::info("FundsUpdate={}"_fmt, event.value);
}

void Strategy::update() {
  if (instrument_.is_marketdata_ready() && 
      model_.update(instrument_) &&
      instrument_.is_trading_ready()) 
  {
      Quote bid = model_.bid();
      Quote ask = model_.ask();
      if(validate_quotes(bid, ask)) {
        bid_.modify(SingleQuote{model_.bid()});
        ask_.modify(SingleQuote{model_.ask()});
      } else {
        bid_.reset();
        ask_.reset();
      }
      bid_.execute();
      ask_.execute();
  } else {
    model_.reset();
    bid_.reset();
    ask_.reset();
    bid_.execute();
    ask_.execute();
  }
}

bool Strategy::validate_quotes(Quote& bid, Quote& ask) {
  if (!limits_.validate(bid, ask)) {
    log::info("Limits *VIOLATED*"_sv);
    return false;
  }  
  return true;
}

template<class ValidateOrder>
bool Strategy::validate_order(const ValidateOrder& order) {
  if(order.quantity()<instrument_.min_trade_vol()+Eps) {
    log::warn("order.quantity < min_trade_vol"_sv);
    return false;
  }
  return true;
}

order_txid_t Strategy::create_order(order_txid_t id, const Order& order) {
  if (!Flags::enable_trading()) {
    log::warn("Trading *NOT* enabled"_sv);
    return {};
  }
  if(!validate_order(order)) {
    return {};
  }
  dispatcher_.send(
    roq::CreateOrder {
          .account = Flags::account(),
          .order_id = id.order_id,          
          .exchange = Flags::exchange(),
          .symbol = Flags::symbol(),
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

order_txid_t Strategy::cancel_order(order_txid_t id) {
  if (!Flags::enable_trading()) {
    log::warn("Trading *NOT* enabled"_sv);
    return {};
  }  
  dispatcher_.send(
    roq::CancelOrder{
        .account = Flags::account(),
        .order_id = id.order_id,
        //.routing_id = existing_order_id.routing_id()
    },
    0u);
  return id;
}

order_txid_t Strategy::modify_order(order_txid_t id, const Order& order) {
  if (!Flags::enable_trading()) {
    log::warn("Trading *NOT* enabled"_sv);
    return {};
  }  
  if(!validate_order(order)) {
    return {};
  }
  dispatcher_.send(
    roq::ModifyOrder{
        .account = Flags::account(),
        .order_id = id.order_id,
        .quantity = order.quantity(),
        .price = order.price(),
        //.routing_id = id.routing_id()
    },
    0u);
  return id;
}

}  // namespace mmaker
}  // namespace roq
