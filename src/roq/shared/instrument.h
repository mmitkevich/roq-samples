/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <array>
#include <limits>
#include <memory>

#include "roq/api.h"
#include <roq/exceptions.h>
#include <roq/reference_data.h>
#include "roq/numbers.h"
#include "roq/client/depth_builder.h"
#include "roq/shared/quote.h"
#include "roq/shared/limits.h"
#include "roq/shared/bitmask.h"
#include "roq/shared/portfolio.h"

namespace roq {
inline namespace shared {

struct Symbol {
  Symbol(const std::string_view& symbol, const std::string_view& exchange)
  : symbol(symbol)
  , exchange(exchange) {}
public:
  std::string_view symbol;
  std::string_view exchange;
};

struct Instrument : Symbol {
  struct ReferenceData {
    bool update(const roq::ReferenceData& data);
    auto tick_size() const { return data_->tick_size; }
    auto min_trade_vol() const { return data_->min_trade_vol; }
    auto multiplier() const { return data_->multiplier; }
    auto& data() { return data_;}
    bool is_ready() const;
  private:
    std::unique_ptr<roq::ReferenceData> data_ = std::make_unique<roq::ReferenceData>();
  };
  
  struct Status {
    bool is_trading_open() const { return trading_status == TradingStatus::OPEN; }
    bool is_auction() const { return trading_status == TradingStatus::CLOSED; }
    bool is_trading_closed() const { return trading_status != TradingStatus::OPEN; }
    
    Status(TradingStatus trading_status=TradingStatus::UNDEFINED)
    : trading_status(trading_status) {}

    bool is_ready() const { return trading_status == TradingStatus::OPEN; }
  public:
    TradingStatus trading_status;
  };

  using Portfolio = roq::shared::Portfolio;

  static const constexpr size_t MAX_DEPTH = 3u;
  using Depth = std::array<Layer, MAX_DEPTH>;

  enum flags_t : uint32_t {
    MARKETDATA        = 1u << 0,
    TRADING           = 1u << 1,
    CONNECTED         = 1u << 2,
    DOWNLOADING       = 1u << 3,
    REALTIME          = 1u << 4,
    READY             = 1u << 30
  };

  Instrument(
      const std::string_view &exchange,
      const std::string_view &symbol);

  Instrument(Instrument &&) = default;
  Instrument(const Instrument &) = delete;

  const Depth &depth() const { return depth_; }

  bool is_marketdata_ready() const { 
    return flags.all(MARKETDATA | CONNECTED | REALTIME);
  }

  bool is_trading_ready() const { 
    return flags.all(MARKETDATA | CONNECTED | REALTIME);
  }

  const ReferenceData& refdata() const { return refdata_; }

  Quote bid() const { 
    return Quote {Side::BUY, depth_[0].bid_price, depth_[0].bid_quantity};
  }

  Quote ask() const { 
    return Quote {Side::SELL, depth_[0].ask_price, depth_[0].ask_quantity};
  }

  const Position& position() const { return position_; }

  void operator()(const roq::Connected &);
  void operator()(const roq::Disconnected &);
  void operator()(const roq::DownloadBegin &);
  void operator()(const roq::DownloadEnd &);
  void operator()(const roq::GatewayStatus &);
  void operator()(const roq::ReferenceData &);
  void operator()(const roq::MarketStatus &);
  void operator()(const roq::MarketByPriceUpdate &);
  void operator()(const roq::MarketByOrderUpdate &);
  //void operator()(const roq::OrderUpdate &);
  void operator()(const roq::PositionUpdate &);

 protected:
  void check_ready();

  void reset();

  void validate(const Depth &);
 
 public:
  shared::BitMask<flags_t> flags {};
 private:
  Account account_;
  Status status_ {};
  ReferenceData refdata_ {};
  Position position_ {};  
  Depth depth_ {};
  std::unique_ptr<client::DepthBuilder> depth_builder_;
  uint32_t last_order_id_ = {};
  double last_traded_quantity_ = {};
  std::size_t ready_counter_ = {};
};


} // namespace shared
} // namespace roq

namespace std {
template<>
struct hash<roq::shared::Symbol> {
  std::size_t operator()(const roq::shared::Symbol& value) {
    return hash<std::string_view>{}(value.symbol) ^ hash<std::string_view>{}(value.exchange);
  }
};
}