/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "instrument.h"
#include <roq/market_status.h>

#include <algorithm>
#include "roq/format.h"
#include "roq/literals.h"

#include "roq/client.h"
#include "roq/logging.h"

#include "roq/utils/compare.h"
#include "roq/utils/mask.h"
#include "roq/utils/update.h"

using namespace roq::literals;

namespace roq {


Instrument::Instrument(
  instrument_id_t id,
  std::string_view symbol,
  std::string_view exchange,
  std::string_view account)
: symbol_(symbol)
, exchange_(exchange)
, account_(account)
{
  post_configure();
}

void Instrument::post_configure() {
  depth_builder_ = client::DepthBuilderFactory::create(symbol_, depth_);
  depth_builder_->reset();
}

#define ROQ_INFO(FMT, ...) roq::log::info(std::string_view("[{}:{}] " FMT), exchange(), symbol(), ##__VA_ARGS__)
#define ROQ_DEBUG(FMT, ...) roq::log::debug(std::string_view("[{}:{}] " FMT), exchange(), symbol(), ##__VA_ARGS__)

void Instrument::operator()(const roq::Connected &) {
  if (flags.set(CONNECTED)) {
    ROQ_INFO("connected={}", exchange(), symbol(), flags.test(CONNECTED));
    check_ready();
  }
}

void Instrument::operator()(const roq::Disconnected &) {
  if (flags.reset(CONNECTED)) {
    ROQ_INFO("connected={}", flags.test(CONNECTED));
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
  ROQ_INFO("downloading");
}

void Instrument::operator()(const roq::DownloadEnd &download_end) {
  if (!download_end.account.empty())  // we only care about market (not account)
    return;
  assert(flags.test(DOWNLOADING));
  flags.reset(DOWNLOADING);
  flags.set(REALTIME);
  ROQ_INFO("realtime");
  check_ready();
}

void Instrument::operator()(const roq::GatewayStatus &gateway_status) {
  ROQ_INFO("gateway_status {}", gateway_status);
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
      ROQ_INFO("market_data");
    if (!flags.test(MARKETDATA)) {
      auto missing = required & ~available;
      ROQ_DEBUG("missing={:#x}", missing.get());
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
       ROQ_INFO("trading");
    // sometimes useful to see what is missing
    if (!flags.test(MARKETDATA)) {
      auto missing = required & ~available;
      ROQ_DEBUG("missing={:#x}");
    }
  }
  // update the ready flag
  check_ready();
}

#define ROQ_FIELD_UPDATE(dst, src, field) \
  if (utils::update((dst).field, (src).field)) \
    log::info(std::string_view{"[{}:{}] {}={}"}, (src).exchange, (src).symbol, #field, (dst).field);

void Instrument::ReferenceData::reset() {
  data_->tick_size = NaN;
  data_->min_trade_vol = NaN;
}

bool Instrument::ReferenceData::update(const roq::ReferenceData& data) {
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
  assert(exchange_.compare(reference_data.exchange) == 0);
  assert(symbol_.compare(reference_data.symbol) == 0);
  assert(depth_builder_);
  // update the depth builder
  depth_builder_->update(reference_data);
  // update cached reference data
  refdata_.update(reference_data);
  // update the ready flag
  check_ready();
}

bool Instrument::Status::update(const MarketStatus& market_status) {
    // update our cache
  return utils::update(trading_status_, market_status.trading_status);
}

void Instrument::operator()(const MarketStatus &market_status) {
  assert(exchange_.compare(market_status.exchange) == 0);
  assert(symbol_.compare(market_status.symbol) == 0);
  if(status_.update(market_status)) {
    ROQ_INFO("status={{trading_status={}}}", status_.trading_status_);
  }
  // update the ready flag
  check_ready();
}

void Instrument::operator()(const MarketByPriceUpdate &market_by_price_update) {
  assert(exchange_.compare(market_by_price_update.exchange) == 0);
  assert(symbol_.compare(market_by_price_update.symbol) == 0);
  if (ROQ_UNLIKELY(flags.test(REALTIME)))
    ROQ_INFO("MarketByPriceUpdate={}", market_by_price_update);
  check_ready();
  depth_builder_->update(market_by_price_update);
  ROQ_DEBUG("depth=[{}]", roq::join(depth_, ", "_sv));
  validate(depth_);
}

void Instrument::operator()(const MarketByOrderUpdate &market_by_order_update) {
  assert(exchange_.compare(market_by_order_update.exchange) == 0);
  assert(symbol_.compare(market_by_order_update.symbol) == 0);
  if (ROQ_UNLIKELY(flags.test(REALTIME)))
    ROQ_INFO("MarketByOrderUpdate={}", market_by_order_update);
  /*
  depth_builder_->update(market_by_order_update);
  log::debug(
      "[{}:{}] depth=[{}]",
      exchange_,
      symbol_,
      roq::join(depth_, ", "_sv));
  validate(depth_);
  */
}

/// default behaviour is to override position
void Instrument::operator()(const roq::PositionUpdate &position_update) {
  assert(account_.compare(position_update.account) == 0);
  ROQ_INFO("position_update={}", exchange(), symbol(), position_update);
  switch (position_update.side) {
    case Side::UNDEFINED:
    case Side::BUY:
    case Side::SELL: {
      position_ = position_update.position;
      ROQ_INFO("position={}", exchange(), symbol(), position_);
    }
    default: {
      log::warn(std::string_view{"Unexpected side={}"}, position_update.side);
    }
  }
}

void Instrument::check_ready() {
  auto before = flags.test(READY);
  if(flags.set(READY, 
    flags.test(CONNECTED) && 
    !flags.test(DOWNLOADING) && 
    refdata_.is_ready() &&
    status_.is_ready() &&
    flags.test(MARKETDATA) &&
    true))
  {
    ROQ_INFO("READY={}", flags.test(READY));
  }
  if(ROQ_UNLIKELY((ready_counter_++)%10)==0) {
    #define ROQ_DEBUG_FIELD(obj,val) ROQ_DEBUG(#val " = {} ", obj.val);

    ROQ_DEBUG_FIELD(refdata_, tick_size());
    ROQ_DEBUG_FIELD(refdata_, min_trade_vol());
    ROQ_DEBUG_FIELD(refdata_, multiplier());
    ROQ_DEBUG_FIELD(flags, test(CONNECTED));
    ROQ_DEBUG_FIELD(flags, test(REALTIME));
    ROQ_DEBUG_FIELD(status_, trading_status());
    ROQ_DEBUG_FIELD(flags, test(MARKETDATA));

    #undef ROQ_DEBUG_FIELD
  }
}

void Instrument::reset() {
  flags.reset();
  refdata_.reset();
  status_.reset();
  assert(depth_builder_);
  depth_builder_->reset();// = client::DepthBuilderFactory::create(symbol, depth_);
  position_.reset();
}

void Instrument::validate(const Depth &depth) {
  if (utils::compare(depth[0].bid_quantity, 0.0) <= 0 ||
      utils::compare(depth[0].ask_quantity, 0.0) <= 0)
    return;
  auto spread = depth[0].ask_price - depth[0].bid_price;
  if (ROQ_UNLIKELY(utils::compare(spread, 0.0) <= 0))
    ROQ_INFO(
        "Probably something wrong: "
        "choice price or price inversion detected. "
        "depth=[{}]",
        exchange(),
        symbol(),
        roq::join(depth, ", "_sv));
}


}  // namespace roq
