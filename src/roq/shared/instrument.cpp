/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "instrument.h"

#include <algorithm>

#include "roq/client.h"
#include "roq/logging.h"

#include "roq/utils/compare.h"
#include "roq/utils/mask.h"
#include "roq/utils/update.h"

using namespace roq::literals;

namespace roq {


Instrument::Instrument(
  const std::string_view &symbol,
  const std::string_view &exchange)
: Symbol(symbol, exchange)
, depth_builder_(client::DepthBuilderFactory::create(symbol, depth_))
{}

void Instrument::operator()(const roq::Connected &) {
  if (flags.set(CONNECTED)) {
    log::info("[{}:{}] connected={}"_fmt, exchange, symbol, flags.test(CONNECTED));
    check_ready();
  }
}

void Instrument::operator()(const roq::Disconnected &) {
  if (flags.reset(CONNECTED)) {
    log::info("[{}:{}] connected={}"_fmt, exchange, symbol, flags.test(CONNECTED));
    // reset all cached state - await download upon reconnection
    reset();
  }
}

void Instrument::operator()(const roq::DownloadBegin &download_begin) {
  if (!download_begin.account.empty())  // we only care about market (not account)
    return;
  assert(!flags.test(DOWNLOADING));
  flags.set(DOWNLOADING);
  flags.reset(REALTIME);
  log::info("[{}:{}] downloading"_fmt, exchange, symbol);
}

void Instrument::operator()(const roq::DownloadEnd &download_end) {
  if (!download_end.account.empty())  // we only care about market (not account)
    return;
  assert(flags.test(DOWNLOADING));
  flags.reset(DOWNLOADING);
  flags.set(REALTIME);
  log::info("[{}:{}] realtime"_fmt, exchange, symbol);
  check_ready();
}

void Instrument::operator()(const roq::GatewayStatus &gateway_status) {
  log::info("gateway_status {}"_fmt, gateway_status);
  utils::Mask<SupportType> available(gateway_status.available),
      unavailable(gateway_status.unavailable);
  if (gateway_status.account.empty()) {
    // bit-mask of required message types
    static const utils::Mask<SupportType> required{
        SupportType::REFERENCE_DATA,
        SupportType::MARKET_STATUS,
        SupportType::MARKET_BY_PRICE,
    };
    bool market_data = available.has_all(required) && unavailable.has_none(required);
    if (flags.set(MARKETDATA, market_data))
      log::info("[{}:{}] market_data"_fmt, exchange, symbol);
    if (!flags.test(MARKETDATA)) {
      auto missing = required & ~available;
      log::debug("missing={:#x}"_fmt, missing.get());
    }
  } else if (account_.compare(gateway_status.account) == 0) {
    // bit-mask of required message types
    static const utils::Mask<SupportType> required{
        SupportType::CREATE_ORDER,
        SupportType::CANCEL_ORDER,
        SupportType::ORDER,
        SupportType::POSITION,
    };
    // readiness defined by full availability of all required message types
    auto trading = available.has_all(required) && unavailable.has_none(required);
    if (flags.set(TRADING, trading))
      log::info("[{}:{}] trading"_fmt, exchange, symbol);
    // sometimes useful to see what is missing
    if (!flags.test(MARKETDATA)) {
      auto missing = required & ~available;
      log::debug("missing={:#x}"_fmt, missing.get());
    }
  }
  // update the ready flag
  check_ready();
}

#define ROQ_FIELD_UPDATE(dst, src, field) \
  if (utils::update((dst).field, (src).field)) \
    log::info("[{}:{}] {}={}"_fmt, (src).exchange, (src).symbol, #field, (dst).field);

bool Instrument::ReferenceData::update(const roq::ReferenceData& data) {
{
  ROQ_FIELD_UPDATE(*data_, data, tick_size);
  ROQ_FIELD_UPDATE(*data_, data, min_trade_vol);
  ROQ_FIELD_UPDATE(*data_, data, multiplier);
  return true;
}

bool Instrument::ReferenceData::is_ready() const {
  return utils::compare(data_->tick_size, 0.) > 0 &&
        utils::compare(data_->min_trade_vol, 0.) > 0 &&
        //utils::compare(multiplier_, 0.) > 0 &&
        true;
}

void Instrument::operator()(const roq::ReferenceData &reference_data) {
  assert(exchange.compare(reference_data.exchange) == 0);
  assert(symbol.compare(reference_data.symbol) == 0);
  // update the depth builder
  depth_builder_->update(reference_data);
  // update cached reference data
  refdata_.update(reference_data);
  // update the ready flag
  check_ready();
}
void Instrument::operator()(const MarketStatus &market_status) {
  assert(exchange_.compare(market_status.exchange) == 0);
  assert(symbol_.compare(market_status.symbol) == 0);
  // update our cache
  if (utils::update(trading_status_, market_status.trading_status)) {
    log::info("[{}:{}] trading_status={}"_fmt, exchange_, symbol_, trading_status_);
  }
  // update the ready flag
  check_ready();
}

void Instrument::operator()(const MarketByPriceUpdate &market_by_price_update) {
  assert(exchange_.compare(market_by_price_update.exchange) == 0);
  assert(symbol_.compare(market_by_price_update.symbol) == 0);
  if (ROQ_UNLIKELY(flags.test(REALTIME)))
    log::info("MarketByPriceUpdate={}"_fmt, market_by_price_update);
  check_ready();
  depth_builder_->update(market_by_price_update);
  log::trace_1("[{}:{}] depth=[{}]"_fmt, exchange, symbol, roq::join(depth_, ", "_sv));
  validate(depth_);
}

void Instrument::operator()(const MarketByOrderUpdate &market_by_order_update) {
  assert(exchange_.compare(market_by_order_update.exchange) == 0);
  assert(symbol_.compare(market_by_order_update.symbol) == 0);
  if (ROQ_UNLIKELY(flags.test(REALTIME)))
    log::info("MarketByOrderUpdate={}"_fmt, market_by_order_update);
  /*
  depth_builder_->update(market_by_order_update);
  log::trace_1(
      "[{}:{}] depth=[{}]"_fmt,
      exchange_,
      symbol_,
      roq::join(depth_, ", "_sv));
  validate(depth_);
  */
}

/* WON'T work for multiple ords
void Instrument::operator()(const OrderUpdate &order_update) {

  switch (order_update.side) {
    case Side::BUY:
      position_ += quantity;
      break;
    case Side::SELL:
      position_ -= quantity;
      break;
    default:
      assert(false);  // unexpected
  }
  log::info("[{}:{}] position={}"_fmt, exchange_, symbol_, position());
}
*/

void Instrument::operator()(const PositionUpdate &position_update) {
  assert(account.compare(position_update.account) == 0);
  log::info("[{}:{}] position_update={}"_fmt, exchange, symbol, position_update);
  if (!flags.get(REALTIME)) {
    // note!
    //   only update positions when downloading
    //   at run-time we're better off maintaining own positions
    //   since the position feed could be broken or very delayed
    switch (position_update.side) {
      case Side::UNDEFINED:
      case Side::BUY:
      case Side::SELL: {
        position_ = position_update.position;
        log::info("[{}:{}] position={}"_fmt, exchange_, symbol_, position_);
      }
      default: {
        log::warn("Unexpected side={}"_fmt, position_update.side);
      }
    }
  } else {
    /*if(std::fabs(position_update.position, position_)>critical_position_imbalance_) {
      // TODO: ALARM!
    }*/
  }
}

void Instrument::check_ready() {
  auto before = flags.test(READY);
  flags.set(READY, 
    flags.test(CONNECTED) && 
    !flags.test(DOWNLOADING) && 
    refdata_.is_ready() &&
    status_.is_ready() &&
    flags.test(MARKETDATA) &&
    true;
    
  if (ROQ_UNLIKELY(ready_ != before))
    log::info("[{}:{}] ready={}"_fmt, exchange_, symbol_, ready_);
  if(ROQ_UNLIKELY((ready_counter_++)%10)==0) {
    #define ROQ_DEBUG_FIELD(s) log::debug("[{}:{}] {} = {} "_fmt, exchange_, symbol_, #s, s);
    ROQ_DEBUG_FIELD(tick_size_);
    ROQ_DEBUG_FIELD(connected_);
    ROQ_DEBUG_FIELD(download_);
    ROQ_DEBUG_FIELD(tick_size_);
    ROQ_DEBUG_FIELD(min_trade_vol_);
    ROQ_DEBUG_FIELD(multiplier_);
    ROQ_DEBUG_FIELD(trading_status_);
    ROQ_DEBUG_FIELD(market_data_);
    ROQ_DEBUG_FIELD(order_management_);
    #undef ROQ_CHECK_FIELD
  }
}

void Instrument::reset() {
  connected_ = false;
  download_ = false;
  tick_size_ = NaN;
  min_trade_vol_ = NaN;
  trading_status_ = {};
  market_data_ = false;
  order_management_ = false;
  depth_builder_->reset();
  position_ = {0.};
  ready_ = false;
  last_order_id_ = {};
  last_traded_quantity_ = {};
}

void Instrument::validate(const Depth &depth) {
  if (utils::compare(depth[0].bid_quantity, 0.0) <= 0 ||
      utils::compare(depth[0].ask_quantity, 0.0) <= 0)
    return;
  auto spread = depth[0].ask_price - depth[0].bid_price;
  if (ROQ_UNLIKELY(utils::compare(spread, 0.0) <= 0))
    log::fatal(
        "[{}:{}] Probably something wrong: "
        "choice price or price inversion detected. "
        "depth=[{}]"_fmt,
        exchange_,
        symbol_,
        roq::join(depth, ", "_sv));
}


}  // namespace roq
