/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>
#include <roq/client/config.h>
#include "roq/client.h"

#include "roq/shared/instrument.h"
#include "roq/shared/grid_order.h"
#include "model.h"
#include "roq/shared/order.h"
#include "roq/shared/vars.h"

namespace roq {
namespace mmaker {

struct Strategy final : client::Handler {
  using InstrumentId = uint32_t;
  
  struct Instrument : roq::Instrument {
    template<class...ArgsT>
    Instrument(Strategy* self, ArgsT...args)
    : roq::Instrument(std::forward<ArgsT>(args)...)
    , self_(self)
    {}
    Strategy* self() { return self_; }
    
    Instrument(Instrument&&) = default;
    Instrument(const Instrument&) = delete;
    bool is_ready() const;
    PositionLimit<Instrument> limits {*this};
  private:
    Strategy* self_ {};
  };

  explicit Strategy(client::Dispatcher &);

  Strategy(Strategy &&) = default;
  Strategy(const Strategy &) = delete;

  template<class ValidateOrder>
  bool validate_order(const ValidateOrder& order);

  //! returns new order_id+routing_id
  order_txid_t create_order(order_txid_t id, const LimitOrder& new_order);
  order_txid_t cancel_order(order_txid_t id, const LimitOrder& order);
  order_txid_t modify_order(order_txid_t id, const LimitOrder& order);

  order_txid_t next_order_txid() { txid_.order_id++; txid_.routing_id_++; return txid_; }
  order_txid_t next_order_txid(order_id_t order_id) { return order_txid_t{order_id, ++txid_.routing_id_}; }

 protected:
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

  // helper - dispatch event to instrument
  template<class Dest, typename T>
  void notify(Dest& dest, const T &event) {
    assert(event.message_info.source == 0u);
    dest(event.value);
  }
  template <typename T>
  void dispatch(const T& event) {
    for(auto& data: data_) {
      notify(data, event);
    }
  }

  bool validate_quotes(Quote& bid, Quote& ask);

  template<class Quotes>
  void modify(const Quotes& quotes) {
    if(quotes.get_side()==Side::BUY) {
      bid_.modify(quotes);
    } else if(quotes.get_side()==Side::SELL) {
      ask_.modify(quotes);
    }
  }
  
  Instrument& instrument(InstrumentId ins) {
    if(ins>=instruments_.size())
      instruments_.resize(ins+1);
    assert(ins>=0);
    assert(ins<instruments_.size());
    return instruments_[ins];
  }
 private:
  client::Dispatcher &dispatcher_;
  std::vector<Instrument> instruments_;
  absl::flat_hash_map<Symbol, InstrumentId> symbol_to_ins_;
  shared::Variables vars_;

  order_txid_t txid_;

  Model model_;

  GridOrder<Self,  1> bid_{this}; 
  GridOrder<Self, -1> ask_{this}; 
  std::chrono::nanoseconds next_sample_ = {};
};


}  // namespace mmaker
}  // namespace roq
