/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <array>

#include "roq/api.h"

#include "roq/shared/instrument.h"
#include "roq/shared/ema.h"

namespace roq {
namespace mmaker {

class Model final {
 public:
  static const constexpr size_t MAX_DEPTH = 3u;

  using Depth = std::array<Layer, MAX_DEPTH>;

  Model() = default;

  Model(Model &&) = default;
  Model(const Model &) = delete;

  void reset() {
      q_bid_.reset();
      q_ask_.reset();
  }

  //! @returns false if nothing has changed
  bool update(const Instrument& ins);
  
  const Quote& bid() const { return q_bid_; }
  const Quote& ask() const { return q_ask_; }
 protected:
  bool validate(const Instrument& ins);
 
 protected:
  Quote q_bid_;
  Quote q_ask_;
  price_t q_spread_ = NaN;
};


}  // namespace mmaker
}  // namespace roq
