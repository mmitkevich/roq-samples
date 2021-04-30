/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <cstdint>
#include <limits>

#include "roq/numbers.h"

namespace roq {
inline namespace shared {
class EMA final {
 public:
  explicit EMA(double alpha, uint32_t warmup=0);

  EMA(EMA &&) = default;
  EMA(const EMA &) = delete;

  operator double() const { return value_; }

  void reset();

  bool is_ready() const { return countdown_ == 0; }

  double update(double value);

 private:
  const double alpha_;
  double value_ = NaN;
  uint32_t countdown_;
  uint32_t warmup_;
};

} // namespace shared
} // namespace roq
