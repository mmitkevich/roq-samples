#pragma once

#include <absl/container/flat_hash_map.h>
#include <roq/client/config.h>
#include "roq/client.h"

#include "roq/shared/instrument.h"
#include "roq/shared/grid_order.h"
#include "roq/shared/order.h"
#include "roq/shared/order_map.h"
#include "roq/shared/type_traits.h"
#include "roq/shared/vars.h"

namespace roq {
inline namespace shared {


// insturments are indexed by integer instrument_id used as plain index for performance
template<class Instrument>
struct Instruments {
  /// find instrument
  /// @returns undefined_instrument_id if not found
  instrument_id_t find_id(const SymbolView& sym) {
    auto sym_it = symbol_to_ins_.find(sym);
    if(sym_it!=symbol_to_ins_.end()) {
      return sym_it->second;
    } else {
      return undefined_instrument_id;
    }
  }
  std::size_t size() const { return instruments_.size(); }

  template<typename T>
  int dispatch(const T& event);
  template<typename T>
  int broadcast(const T& event);

  /// get or add instrument
  Instrument& operator[](const SymbolView& sym) {
    instrument_id_t iid = find_id(sym);
    if(iid == undefined_instrument_id) {
      symbol_to_ins_.emplace(sym, iid);
      return instruments_.emplace_back(Instrument(iid, sym.symbol(), sym.exchange()));
    }
    return instruments_[iid];
  }
  /// get instrument by index
  Instrument& operator[](instrument_id_t id) {
    assert(id!=undefined_instrument_id);
    instruments_.resize(std::size_t(id)+1);
    return instruments_[id];
  }
private:
  absl::flat_hash_map<SymbolView, instrument_id_t> symbol_to_ins_;
  std::vector<Instrument> instruments_;
};

template<template<int Dir> class Order>
struct QuotingInstrument: roq::shared::Instrument {
    using roq::shared::Instrument::Instrument;
    bool validate_order(LimitOrder& order);
    using roq::shared::Instrument::operator();
    Order<1>& buy_order() { return buy_order_; }
    Order<-1>& sell_order() { return sell_order_; }
    
    void operator()(const OrderUpdate &order_update) {
      switch(order_update.side) {
        case Side::BUY:  buy_order_.order_updated(order_update); break;
        case Side::SELL: sell_order_.order_updated(order_update); break;
        default: assert(false);
      }
    }
  private:
    PositionLimit<Instrument> limits_ {*this};
    LimitOrdersMap orders_;      
    Order<1> buy_order_{orders_};
    Order<-1> sell_order_{orders_};
};

template<class Self, class Instrument>
struct Strategy : client::Handler {
  Self* self() { return static_cast<Self*>(this); }

  explicit Strategy(client::Dispatcher& dispatcher)
  : dispatcher_(dispatcher)
  {}

  // Derivied should define:

  // Model model();
  // Config config();

  Strategy(Strategy &&) = default;
  Strategy(const Strategy &) = delete;

  //! returns new order_id+routing_id
  order_txid_t create_order(order_txid_t id, const LimitOrder& new_order);
  order_txid_t cancel_order(order_txid_t id, const LimitOrder& order);
  order_txid_t modify_order(order_txid_t id, const LimitOrder& order);

  order_txid_t next_order_txid() { txid_.order_id++; txid_.routing_id_++; return txid_; }
  order_txid_t next_order_txid(order_id_t order_id) { return order_txid_t{order_id, ++txid_.routing_id_}; }

  ROQ_DECLARE_HAS_MEMBER(symbol);
  ROQ_DECLARE_HAS_MEMBER(exchange);

  template<class T>
  int dispatch(const T& event) {
    if constexpr(ROQ_HAS_MEMBER(symbol, T) && ROQ_HAS_MEMBER(exchange, T)) {
      return instruments_.dispatch(event);
    } else {
      return instruments_.broadcast(event);
    }
  }

  template<class Quotes>
  void modify_orders(Instrument& ins, const Quotes& bid, const Quotes& ask) {
    ins.buy_order().modify(bid);
    ins.sell_order().modify(ask);
  }
public:
  void operator()(const Event<Timer> &) override;
  void operator()(const Event<Connected> &) override;
  void operator()(const Event<Disconnected> &) override;
  void operator()(const Event<DownloadBegin> &) override;
  void operator()(const Event<DownloadEnd> &) override;
  void operator()(const Event<GatewayStatus> &) override;
  void operator()(const Event<ReferenceData> &) override;
  void operator()(const Event<MarketStatus> &) override;
  void operator()(const Event<MarketByPriceUpdate> &) override;
  void operator()(const Event<OrderAck> &) override;
  void operator()(const Event<OrderUpdate> &) override;
  void operator()(const Event<TradeUpdate> &) override;
  void operator()(const Event<PositionUpdate> &) override;
  void operator()(const Event<FundsUpdate> &) override;

  auto sample_freq() { return std::chrono::seconds{1}; }
  
  bool validate_quotes(Quote& bid, Quote& ask);
  Instruments<Instrument>& instruments() { return instruments_; }
 private:
  bool cache_all_instruments_ = true;
  Instruments<Instrument> instruments_; 
  client::Dispatcher &dispatcher_;
  LimitOrdersMap orders_;
  shared::Variables vars_;
  order_txid_t txid_;
  std::chrono::nanoseconds next_sample_ = {};
};


}  // namespace shared
}  // namespace roq

#include "roq/shared/strategy.inl"