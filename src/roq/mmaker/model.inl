/* Copyright (c) 2017-2021, Hans Erik Thrane */
#include <numeric>
#include "model.h"
#include "roq/logging.h"
#include "roq/shared/model.inl"

namespace roq {
namespace mmaker {

using namespace roq::literals;

/// quotes for instrument has been updated
template<class Strategy>
void Model::quotes_updated(Strategy& strategy, instrument_id_t iid) {
  if(iid!=QUOTING)
    return;

  auto& ins = strategy.instruments()[iid];
  auto tick = ins.refdata().tick_size();
  auto qty = ins.refdata().min_trade_vol();
  
  Quote buy { 
    .side_ = Side::BUY,
    .price_ =  ins.bid().price() - tick*100, 
    .quantity_ = qty
  };
  Quote sell {
    .side_ = Side::SELL,
    .price_ = ins.ask().price() + tick*100,
    .quantity_ = qty
  };
  log::debug("model: {{ bid:{}, ask:{}, tick:{}, buy:{} sell:{} }}"_fmt,
    ins.bid(), ins.ask(), tick, buy, sell);
  
  strategy.modify_orders(ins, buy, sell);
}

template<class Strategy>
bool Model::validate(Strategy& strategy, instrument_id_t iid) {
  if(iid==QUOTING)
    return true;
  if(!has_bid_ask(strategy, iid)) return false; // require bid ask for all instruments except quoted one
}



}  // namespace mmaker
}  // namespace roq
