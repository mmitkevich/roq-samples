/* Copyright (c) 2017-2021, Hans Erik Thrane */
#pragma once

#include <numeric>
#include "model.h"
#include "roq/logging.h"
#include "roq/shared/model.inl"

namespace roq {
namespace mmaker {

using namespace roq::literals;

template<class Config>
void Model::configure(const Config& config) {
  quoting_spread_ = config.double_value("quoting_spread");
}
  
/// quotes for instrument has been updated
template<class Strategy>
void Model::quotes_updated(Strategy& strategy, instrument_id_t iid) {
  if(iid!=QUOTING)
    return;

  auto& ins = strategy.instruments()[iid];
  auto qty = quoting_qty(ins); 
  auto spread = quoting_spread(ins);
  
  Quote buy { 
    .side_ = Side::BUY,
    .price_ =  ins.bid().price() - 0.5 * quoting_spread(ins), 
    .quantity_ = qty
  };
  Quote sell {
    .side_ = Side::SELL,
    .price_ = ins.ask().price() + 0.5 * quoting_spread(ins),
    .quantity_ = qty
  };
  
  log::debug("model: {{ bid:{}, ask:{}, tick:{}, buy:{} sell:{} }}"_sv,
    ins.bid(), ins.ask(), ins.refdata().tick_size(), buy, sell);
  
  strategy.modify_buy_order(ins, buy);
  strategy.modify_sell_order(ins, sell);
}

template<class Strategy>
bool Model::validate(Strategy& strategy, instrument_id_t iid) {
  if(iid==QUOTING)
    return true;
  if(!has_bid_ask(strategy, iid)) return false; // require bid ask for all instruments except quoted one
}



}  // namespace mmaker
}  // namespace roq
