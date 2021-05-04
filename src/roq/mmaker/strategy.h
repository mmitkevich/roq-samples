/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include "roq/client.h"

#include "roq/shared/instrument.h"
#include "roq/shared/grid_order.h"
#include "model.h"
#include "roq/shared/order.h"

namespace roq {
namespace mmaker {


class Strategy final : public client::Handler {
 public:
  explicit Strategy(client::Dispatcher &);

  Strategy(Strategy &&) = default;
  Strategy(const Strategy &) = delete;

  template<class ValidateOrder>
  bool validate_order(const ValidateOrder& order);

  //! returns new order_id+routing_id
  order_txid_t create_order(order_txid_t id, const LimitOrder& new_order);
  order_txid_t cancel_order(order_txid_t id, const LimitOrder& order);
  order_txid_t modify_order(order_txid_t id, const LimitOrder& order);

  order_txid_t next_order_txid() { return order_txid_t{++max_order_id_, ++max_client_order_id_}; }
  order_txid_t next_order_txid(order_id_t order_id) { return order_txid_t{order_id, ++max_client_order_id_}; }

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
  template <typename T>
  void dispatch(const T &event) {
    assert(event.message_info.source == 0u);
    instrument_(event.value);
  }

  bool validate_quotes(Quote& bid, Quote& ask);

  void update();

 private:
  client::Dispatcher &dispatcher_;
  Instrument instrument_;
  PositionLimit<Instrument> limits_{instrument_};

  order_id_t max_order_id_ = 0;
  order_id_t max_client_order_id_ = 0;

  Model model_;
  using Self = Strategy;
  GridOrder<Self,  1> bid_{this}; 
  GridOrder<Self, -1> ask_{this}; 
  std::chrono::nanoseconds next_sample_ = {};
};


}  // namespace mmaker
}  // namespace roq
