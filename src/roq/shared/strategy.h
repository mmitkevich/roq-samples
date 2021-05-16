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
namespace shared {

template<class Self>
struct Strategy : client::Handler {
  Self* self() { return static_cast<Self*>(this); }
  struct Instrument : roq::shared::Instrument {
      template<class...ArgsT>
      Instrument(Self& context, ArgsT...args)
      : roq::shared::Instrument(std::forward<ArgsT>(args)...)
      , context_(context) {}

      bool validate_order(LimitOrder& order);
      using roq::shared::Instrument::operator();
      void operator()(const roq::OrderUpdate &);
      GridOrder<1>& buy_order() { return buy_order_; }
      GridOrder<-1>& sell_order() { return sell_order_; }
    private:
      PositionLimit<Instrument> limits_ {*this};
      GridOrder<1> buy_order_;
      GridOrder<-1> sell_order_;
      Self& context_;
  };

  using Instruments = std::vector<Instrument>;

  explicit Strategy(client::Dispatcher& dispatcher)
  : dispatcher_(dispatcher) {}

  Strategy(Strategy &&) = default;
  Strategy(const Strategy &) = delete;

  //! returns new order_id+routing_id
  order_txid_t create_order(order_txid_t id, const LimitOrder& new_order);
  order_txid_t cancel_order(order_txid_t id, const LimitOrder& order);
  order_txid_t modify_order(order_txid_t id, const LimitOrder& order);

  order_txid_t next_order_txid() { txid_.order_id++; txid_.routing_id_++; return txid_; }
  order_txid_t next_order_txid(order_id_t order_id) { return order_txid_t{order_id, ++txid_.routing_id_}; }

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

  
  template<class T>
  instrument_id_t to_instrument_id (const T& event) {
    auto sym_it = symbol_to_ins_.find(Symbol{event.symbol, event.exchange});
    if(sym_it!=symbol_to_ins_.end()) {
      return sym_it->second;
    } else {
      return undefined_instrument_id;
    }
  }

  auto sample_freq() { return std::chrono::seconds{1}; }
  
  template<typename T>
  void dispatch(const T& event);

  bool validate_quotes(Quote& bid, Quote& ask);
  Instruments& instruments() { return instruments_; }
 private:
  Instruments instruments_; 
  client::Dispatcher &dispatcher_;
  OrderMap orders_;
  absl::flat_hash_map<Symbol, instrument_id_t> symbol_to_ins_;
  shared::Variables vars_;
  order_txid_t txid_;
  std::chrono::nanoseconds next_sample_ = {};
};


}  // namespace shared
}  // namespace roq

#include "roq/shared/strategy.inl"