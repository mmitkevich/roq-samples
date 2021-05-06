/* Copyright (c) 2017-2021, Hans Erik Thrane */
#include <numeric>
#include "model.h"
#include "roq/logging.h"

namespace roq {
namespace mmaker {

using namespace roq::literals;
template<class Strategy>
void Model::quotes_updated(Strategy& strat, const Instrument& ins) {
  const auto& depth = ins.depth();
  auto tick = ins.refdata().tick_size();
  auto qty = ins.refdata().min_trade_vol();
  Quote qbid { 
    .side = Side::BUY,
    .price =  ins.bid().price - tick*100, 
    .quantity = qty
  };
  Quote qask {
    .side = Side::SELL,
    .price = ins.ask().price + tick*100,
    .quantity = qty
  };
  log::debug(
    "model: {{ bid:{}, ask:{}, tick:{}, qbid:{} qask:{} }}"_fmt,
    ins.bid(),
    ins.ask(),
    tick,
    qbid,
    qask);
  strat.modify(qbid);
  strat.modify(qask);
}

bool Model::validate(const Instrument& ins) {  // require full depth
  const auto& depth = ins.depth();
  return std::accumulate(depth.begin(), depth.end(), true, [](bool value, const Layer &layer) {
    return value && utils::compare(layer.bid_quantity, 0.0) > 0 &&
           utils::compare(layer.ask_quantity, 0.0) > 0;
  });
}

}  // namespace mmaker
}  // namespace roq
