#pragma once
#include "roq/shared/model.h"


namespace roq {
namespace shared {

template<class Context>
bool Model::has_bid_ask(instrument_id_t iid, Context& context) {  // require full depth
  auto& ins = context.instruments()[iid];
  const auto& depth = ins.depth();
  return std::accumulate(depth.begin(), depth.end(), true, [](bool value, const Layer &layer) {
    return value && utils::compare(layer.bid_quantity, 0.0) > 0 &&
           utils::compare(layer.ask_quantity, 0.0) > 0;
  });
}
}
}