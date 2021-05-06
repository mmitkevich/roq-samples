/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <array>
#include <vector>

#include "roq/api.h"

#include "roq/shared/instrument.h"
#include "roq/shared/ema.h"

namespace roq {
namespace mmaker {

// Model calculates quotes for single instrument
// Model uses quotes from many instruments
// Instruments 
class Model final {
 public:
  static const constexpr size_t MAX_DEPTH = 3u;

  using Depth = std::array<Layer, MAX_DEPTH>;

  Model() = default;

  Model(Model &&) = default;
  Model(const Model &) = delete;

  template<class Strategy>
  void quotes_updated(Strategy& strat, const Instrument& ins);

 protected:
  bool validate(const Instrument& ins);
 
 protected:
  std::vector<Quote> bids;
  std::vector<Quote> asks;
  Quote quote_bid;
  Quote quote_ask;
  price_t q_spread_ = NaN;
};


}  // namespace mmaker
}  // namespace roq
