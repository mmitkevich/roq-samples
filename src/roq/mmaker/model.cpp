/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "model.h"

#include <numeric>

#include "roq/logging.h"

#include "roq/utils/compare.h"

#include "flags.h"

using namespace roq::literals;

namespace roq {
namespace mmaker {

bool Model::update(const Instrument& ins) {
  const auto& depth = ins.depth();
  q_bid_.price = depth[0].bid_price - ins.tick_size()*100;
  q_ask_.price = depth[0].ask_price + ins.tick_size()*100;
    log::debug(
      "model={{"
      "bid={}:{} "
      "ask={}:{} "
      "tick={} "
      "q_bid={} "
      "q_sell={} "
      "}}"_fmt,
      depth[0].bid_price,depth[0].bid_quantity,
      depth[0].ask_price,depth[0].ask_quantity,
      ins.tick_size(),
      q_bid_,
      q_ask_);
  return true;
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
