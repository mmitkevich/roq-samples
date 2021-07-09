/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <array>
#include <limits>
#include <memory>

#include "roq/api.h"
#include <roq/create_order.h>
#include <roq/exceptions.h>
#include <roq/market_status.h>
#include <roq/order_update.h>
#include <roq/reference_data.h>
#include "roq/numbers.h"
#include "roq/client/depth_builder.h"
#include "roq/shared/quote.h"
#include "roq/shared/limits.h"
#include "roq/shared/bitmask.h"
#include "roq/shared/portfolio.h"

namespace roq {
inline namespace shared {

struct SymbolView;

struct SymbolView {
  SymbolView(std::string_view symbol, std::string_view exchange)
  : symbol_(std::move(symbol))
  , exchange_(std::move(exchange)) {}
 
  template<class T>
  static SymbolView from(const T& event);

  template<class RHS>
  friend inline bool operator==(const SymbolView& lhs, const RHS& rhs) {
    return lhs.symbol() == rhs.symbol() && lhs.exchange() == rhs.exchange();
  }
  std::string_view symbol() const { return symbol_; }
  std::string_view exchange() const { return symbol_; }
public:
  std::string_view symbol_;
  std::string_view exchange_;
};

namespace detail {
template<class T>
struct SymbolView_from {
  SymbolView operator()(T& value);
};
template<class T>
struct SymbolView_from<Event<T>> {
  SymbolView operator()(const Event<T> & event) {
    return SymbolView(event.value.symbol, event.value.exchange);
  }
};
}
template<class T>
SymbolView SymbolView::from(const T& event) { return detail::SymbolView_from<T>{}(event); }

using instrument_id_t = int32_t;
constexpr static instrument_id_t undefined_instrument_id = -1;

struct Instrument {
  struct ReferenceData {
    void reset();
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
    Status(TradingStatus trading_status=TradingStatus::UNDEFINED)
    : trading_status_(trading_status) {}
    
    bool is_trading_open() const { return trading_status_ == TradingStatus::OPEN; }
    bool is_auction() const { return trading_status_ == TradingStatus::CLOSE; }
    bool is_trading_closed() const { return trading_status_ != TradingStatus::OPEN; }
    
    void reset() { trading_status_ = TradingStatus::UNDEFINED; }
    TradingStatus trading_status() const { return trading_status_; }    

    bool update(const MarketStatus& update);
    bool is_ready() const { return trading_status_ == TradingStatus::OPEN; }
  public:
    TradingStatus trading_status_;
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
    VWAP              = 1u << 8,  // use vwap prices instead of best for "quotes"
    READY             = 1u << 30
  };

  Instrument(
      instrument_id_t iid,
      std::string_view symbol,    
      std::string_view exchange,
      std::string_view account={}
      );

  Instrument() = default;
  
  Instrument(Instrument &&) = default;
  Instrument(const Instrument &) = delete;

  const Depth &depth() const { return depth_; }

  bool is_marketdata_ready() const { 
    return flags.all(MARKETDATA | CONNECTED | REALTIME);
  }

  bool is_trading_ready() const { 
    return flags.all(MARKETDATA | CONNECTED | REALTIME);
  }

  instrument_id_t id() const { return id_; }
  void set_id(instrument_id_t val) { id_ = val; }

  std::string_view symbol() const { return symbol_; }
  std::string_view exchange() const { return exchange_; }

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
  void operator()(const roq::OrderUpdate &);
  void operator()(const roq::PositionUpdate &);

  template<class Order>
  bool validate_order(const Order&) { return true; }

  void reset();
 protected:
  void check_ready();
  void validate(const Depth &);
 
 public:
  shared::BitMask<flags_t> flags {};
 private:
  instrument_id_t id_;
  std::string symbol_;
  std::string exchange_;
  Account account_;
  Status status_ {};
  ReferenceData refdata_ {};
  Position position_ {};  
  Depth depth_ {};
  std::unique_ptr<client::DepthBuilder> depth_builder_;
  std::size_t ready_counter_ = {};
};



} // namespace shared
} // namespace roq

namespace std {
template<>
struct hash<roq::shared::SymbolView> {
  std::size_t operator()(const roq::shared::SymbolView& value) {
    return hash<std::string_view>{}(value.symbol()) ^ hash<std::string_view>{}(value.exchange());
  }
};
}